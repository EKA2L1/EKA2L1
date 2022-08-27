/*
 * Copyright (c) 2022 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/internet/protocols/common.h>
#include <services/internet/protocols/inet.h>

#include <common/random.h>
#include <common/log.h>
#include <common/algorithm.h>
#include <config/config.h>
#include <common/upnp.h>

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::bt {
    static constexpr std::uint32_t TIMEOUT_HEARING_STRANGER_MS = 2000;
    static constexpr std::uint16_t CENTRAL_SERVER_STANDARD_PORT = 27138;
    static constexpr std::uint16_t HARBOUR_PORT = 35689;

    bool midman_inet::should_upnp_apply_to_port() {
        return enable_upnp_ && (discovery_mode_ == DISCOVERY_MODE_PROXY_SERVER) && (server_addr_.family_ == internet::INET_ADDRESS_FAMILY);
    }

    midman_inet::midman_inet(const config::state &conf)
        : midman()
        , allocated_ports_(MAX_PORT)
        , port_offset_(conf.btnet_port_offset)
        , enable_upnp_(conf.enable_upnp)
        , friend_info_cached_(false)
        , virt_bt_info_server_(nullptr)
        , virt_server_socket_(nullptr)
        , hearing_timeout_timer_(nullptr)
        , port_(conf.internet_bluetooth_port)
        , current_active_observer_(nullptr)
        , password_(conf.btnet_password)
        , discovery_mode_(static_cast<discovery_mode>(conf.btnet_discovery_mode)) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if (discovery_mode_ != DISCOVERY_MODE_DIRECT_IP) {
            port_ = HARBOUR_PORT;
        }

        std::fill(port_refs_.begin(), port_refs_.end(), 0);

        std::vector<std::uint64_t> errs;
        update_friend_list(conf.friend_addresses, errs);

        for (std::size_t i = 0; i < errs.size(); i++) {
            std::uint32_t index = static_cast<std::uint32_t>(errs[i] & 0xFFFFFFFF);
            if (errs[i] & FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Bluetooth netplay friend address number {} has invalid port number ({})", index + 1, conf.friend_addresses[index].port_);
            } else if (errs[i] & FRIEND_UPDATE_ERROR_INVALID_ADDR) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Bluetooth netplay friend address number {} has invalid address ({})", index + 1, conf.friend_addresses[index].addr_);
            }
        }

        for (std::uint32_t i = 0; i < 6; i++) {
            random_device_addr_.addr_[i] = static_cast<std::uint8_t>(random_range(0, 0xFF));
        }

        uv_udp_t *virt_bt_info_server_detail = new uv_udp_t;
        virt_bt_info_server_ = virt_bt_info_server_detail;

        if (uv_udp_init(uv_default_loop(), virt_bt_info_server_detail) < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to initialize INET Bluetooth info server!");
        }
        
        uv_timer_t *timeout_timer = new uv_timer_t;
        hearing_timeout_timer_ = timeout_timer;

        uv_timer_init(uv_default_loop(), timeout_timer);

        virt_bt_info_server_detail->data = this;

        if (discovery_mode_ == DISCOVERY_MODE_LOCAL_LAN) {
            if (!epoc::internet::retrieve_local_ip_info(server_addr_, &local_addr_)) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Can't find local LAN interface for BT netplay!");
                return;
            }

            local_addr_.port_ = static_cast<std::uint16_t>(port_);
        } else if (discovery_mode_ == DISCOVERY_MODE_PROXY_SERVER) {
            addrinfo *result_info = nullptr;
            addrinfo hint_info;
            std::memset(&hint_info, 0, sizeof(addrinfo));

            hint_info.ai_family = AF_UNSPEC;
            hint_info.ai_socktype = SOCK_STREAM;
            hint_info.ai_protocol = IPPROTO_TCP;

            addrinfo *ideal_result_info = nullptr;

            int result_code = getaddrinfo(conf.bt_central_server_url.c_str(), nullptr, &hint_info, &result_info);
            if (result_code != 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Can't resolve central server address for discovery!");

                freeaddrinfo(result_info);
                return;
            } else {
                ideal_result_info = result_info;
                while (ideal_result_info != nullptr) {
                    if ((ideal_result_info->ai_family == AF_INET6) && ((ideal_result_info->ai_flags & AI_V4MAPPED) == 0)) {
                        break;
                    }

                    ideal_result_info = ideal_result_info->ai_next;
                }

                if (!ideal_result_info) {
                    ideal_result_info = result_info;
                }
            }

            epoc::socket::name_entry entry;
            epoc::internet::addrinfo_to_name_entry(entry, ideal_result_info);

            server_addr_ = entry.addr_;
            server_addr_.port_ = CENTRAL_SERVER_STANDARD_PORT;

            // Send a login package            
            uv_tcp_t *new_tcp_meta = new uv_tcp_t;
            virt_server_socket_ = new_tcp_meta;

            uv_tcp_init(uv_default_loop(), new_tcp_meta);

            new_tcp_meta->data = this;

            struct meta_server_conn_data {
                uv_tcp_t *new_tcp_meta_;
                sockaddr_in6 addr_;
            };

            meta_server_conn_data *conn_data = new meta_server_conn_data;
            conn_data->new_tcp_meta_ = new_tcp_meta;
            std::memcpy(&conn_data->addr_, ideal_result_info->ai_addr, sizeof(sockaddr_in6));

            conn_data->addr_.sin6_port = htons(CENTRAL_SERVER_STANDARD_PORT);

            uv_async_t *async = new uv_async_t;
            async->data = conn_data;
            
            uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
                meta_server_conn_data *data_bunch = reinterpret_cast<decltype(data_bunch)>(async->data);
                uv_tcp_t *new_tcp_meta = reinterpret_cast<uv_tcp_t*>(data_bunch->new_tcp_meta_);
                uv_connect_t *connect = new uv_connect_t;
                connect->data = new_tcp_meta->data;

                sockaddr_in6 sock_addr_conn = data_bunch->addr_;
                delete data_bunch;

                sockaddr_in6 addr_temp;
                std::memset(&addr_temp, 0, sizeof(sockaddr_in6));

                addr_temp.sin6_family = sock_addr_conn.sin6_family;
                uv_tcp_bind(new_tcp_meta, reinterpret_cast<const sockaddr*>(&addr_temp), 0);

                int name_len = sizeof(addr_temp);
                uv_tcp_getsockname(new_tcp_meta, reinterpret_cast<sockaddr*>(&addr_temp), &name_len);

                int err = uv_tcp_connect(connect, new_tcp_meta, reinterpret_cast<sockaddr*>(&sock_addr_conn), [](uv_connect_t *conn, int status) {
                    // NOTE: Reuse this because they are expected to be in same loop!
                    uv_read_start(conn->handle, [](uv_handle_t* handle, std::size_t suggested_size, uv_buf_t* buf_ptr) {
                        reinterpret_cast<midman_inet*>(handle->data)->prepare_server_recv_buffer(buf_ptr, suggested_size);
                    }, [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf_ptr) {
                        reinterpret_cast<midman_inet*>(stream->data)->handle_meta_server_msg(static_cast<std::int64_t>(nread), buf_ptr);
                    });

                    reinterpret_cast<midman_inet*>(conn->data)->send_login();
                    delete conn;
                });

                if (err < 0) {
                    LOG_ERROR(SERVICE_BLUETOOTH, "Fail to connect to central Bluetooth Netplay server! Libuv's error code {}", err);
                    delete connect;
                }

                uv_close(reinterpret_cast<uv_handle_t*>(async), [](uv_handle_t *hh) {
                    uv_async_t *async = reinterpret_cast<uv_async_t*>(hh);
                    delete async;
                });
            });

            uv_async_send(async);
            freeaddrinfo(result_info);
        }

        sockaddr_in6 addr_bind;
        std::memset(&addr_bind, 0, sizeof(sockaddr_in6));
        addr_bind.sin6_family = (discovery_mode_ == DISCOVERY_MODE_LOCAL_LAN) ? AF_INET : AF_INET6;
        addr_bind.sin6_port = htons(static_cast<std::uint16_t>(port_));

        uv_udp_bind(virt_bt_info_server_detail, reinterpret_cast<const sockaddr*>(&addr_bind), 0);

        if (should_upnp_apply_to_port()) {
            UPnP::TryPortmapping(static_cast<std::uint16_t>(port_), true);
        }

        uv_udp_recv_start(virt_bt_info_server_detail, [](uv_handle_t* handle, std::size_t suggested_size, uv_buf_t* buf) {
            reinterpret_cast<midman_inet*>(handle->data)->prepare_server_recv_buffer(buf, suggested_size);
        }, [](uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
            if (nread <= 0) {
                return;
            }

            reinterpret_cast<midman_inet*>(handle->data)->handle_server_request(addr, buf, nread);
        });
    }

    midman_inet::~midman_inet() {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        uv_async_t *async = new uv_async_t;

        if (should_upnp_apply_to_port()) {
            UPnP::StopPortmapping(static_cast<std::uint16_t>(port_), true);

            for (std::size_t i = 0; i < port_refs_.size(); i++) {
                if (port_refs_[i] != 0) {
                    UPnP::StopPortmapping(static_cast<std::uint16_t>(port_offset_ + i), false);
                }
            }
        }

        struct handle_close_collection {
            uv_udp_t *server_;
            uv_timer_t *timer_;
            uv_tcp_t *tcp_meta_glob_;
        };

        handle_close_collection *close_col = new handle_close_collection;
        close_col->server_ = reinterpret_cast<uv_udp_t*>(virt_bt_info_server_);
        close_col->timer_ = reinterpret_cast<uv_timer_t*>(hearing_timeout_timer_);
        close_col->tcp_meta_glob_ = reinterpret_cast<uv_tcp_t*>(virt_server_socket_);

        async->data = close_col;

        uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
            handle_close_collection *close_col = reinterpret_cast<handle_close_collection*>(async->data);

            uv_udp_recv_stop(close_col->server_);
            uv_close(reinterpret_cast<uv_handle_t*>(close_col->server_), [](uv_handle_t *hh) {
                delete hh;
            });

            uv_timer_stop(close_col->timer_);
            uv_close(reinterpret_cast<uv_handle_t*>(close_col->timer_), [](uv_handle_t *hh) {
                delete hh;
            });

            midman_inet::send_logout(close_col->tcp_meta_glob_, true);

            delete close_col;
            uv_close(reinterpret_cast<uv_handle_t*>(async), [](uv_handle_t *hh) {
                delete hh;
            });
        });

        uv_async_send(async);
    }

    void midman_inet::prepare_server_recv_buffer(void *buf_void, const std::size_t suggested_size) {
        uv_buf_t *buf = reinterpret_cast<uv_buf_t*>(buf_void);
        server_recv_buf_.resize(suggested_size);
        
        buf->base = server_recv_buf_.data();
        buf->len = static_cast<std::uint32_t>(suggested_size);
    }

    void midman_inet::reset_friend_timeout_timer(uv_timer_t *timer) {
        uv_timer_start(timer, [](uv_timer_t *timer) {
            uv_timer_stop(timer);

            midman_inet *mid = reinterpret_cast<midman_inet*>(timer->data);
            const std::lock_guard<std::mutex> guard(mid->friends_lock_);

            if (mid->current_active_observer_) {
                mid->current_active_observer_->on_no_more_strangers();
                mid->current_active_observer_ = nullptr;

                if (!mid->pending_observers_.empty()) {
                    inet_stranger_call_observer *obs = mid->pending_observers_.front();
                    mid->pending_observers_.erase(mid->pending_observers_.begin());

                    mid->current_active_observer_ = obs;
                    mid->send_call_for_strangers();
                }
            }
        }, TIMEOUT_HEARING_STRANGER_MS, 0);
    }

    // Protocol:
    // Note: lb = lower byte, number next to it is index of the byte
    // Packet                           |   Description + Response
    // n                                |   Return name of the device
    // a                                |   Return virtual Bluetooth address of the emulated device
    // pe<lb0><lb1><lb2><lb3>           |   Check if a real port is mapped to a virtual port. Used for differentiate devices with same IP
    //                                  |   Return 1 if true, 0 if false
    // pl<lb0><lb1>                     |   Get the real port that a virtual port is mapped to. Return 32-bit port integer
    // cr<pass_length><pass_bytes>      |   Packet requested calling any device with the same password phrase to return back
    // <ip_type><ip_bytes>              |   Packet response layout, see the "ca" packet.
    //                                  |   If the current BT netplay model is foreign proxy server,
    //                                  |   the answering IP is returned after the password.
    // ca                               |   Answering a device discovery call.                           
    void midman_inet::handle_server_request(const sockaddr *requester, const void *buf_void, std::int64_t nread) {
        if (discovery_mode_ == DISCOVERY_MODE_LOCAL_LAN) {
            if (memcmp(&(reinterpret_cast<const sockaddr_in*>(requester)->sin_addr), local_addr_.user_data_, 4) == 0) {
                return;
            }
        }

        const uv_buf_t *buf = reinterpret_cast<const uv_buf_t*>(buf_void);

        uv_udp_send_t *send_req = new uv_udp_send_t;
        uv_buf_t local_buf;
        std::uint32_t temp_uint;
        char temp_char;
        std::vector<char> temp_data;

        sockaddr_in6 another_addr;
        
        if (buf->base[0] == 'n') {
            std::string device_name_utf8 = common::ucs2_to_utf8(device_name());
            local_buf = uv_buf_init(device_name_utf8.data(), static_cast<std::uint32_t>(device_name_utf8.length()));
        } else if (buf->base[0] == 'p') {
            if (buf->base[1] == 'e') {
                std::uint32_t requested_port = static_cast<std::uint8_t>(buf->base[2]) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf->base[3])) << 8) | 
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf->base[4])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf->base[5])) << 24);
                temp_char = '0';

                if ((requested_port >= port_offset_) && (requested_port < port_offset_ + MAX_PORT)) {
                    if (allocated_ports_.is_allocated(requested_port - port_offset_)) {
                        temp_char = '1';
                    }
                }

                local_buf = uv_buf_init(&temp_char, 1);
            } else if (buf->base[1] == 'l') {
                std::uint16_t virtual_port = buf->base[2] | (static_cast<std::uint16_t>(buf->base[3]) << 8);
                temp_uint = lookup_host_port(virtual_port);

                local_buf = uv_buf_init(reinterpret_cast<char*>(&temp_uint), 4);
            }
        } else if (buf->base[0] == 'a') {
            local_buf = uv_buf_init(reinterpret_cast<char*>(&random_device_addr_), sizeof(random_device_addr_));
        } else if (buf->base[0] == 'c') {
            // And another number 7. siuuuuuuuuuuu,,,,...
            if (buf->base[1] == 'r') {
                bool accept_call = false;

                if (discovery_mode_ == DISCOVERY_MODE_LOCAL_LAN) {
                    char pass_len = static_cast<char>(buf->base[2]);
                    if (pass_len != 0) {
                        if (password_ == std::string(buf->base + 3, pass_len)) {
                            accept_call = true;
                        }
                    } else {
                        accept_call = true;
                    }
                } else {
                    accept_call = true;
                }

                if (accept_call) {
                    temp_data.push_back('c');
                    temp_data.push_back('a');

                    local_buf = uv_buf_init(temp_data.data(), static_cast<unsigned int>(temp_data.size()));
                } else {
                    return;
                }

                if (discovery_mode_ == DISCOVERY_MODE_PROXY_SERVER) {
                    char is_ipv4 = (buf->base[2] == '0');
                    char is_ipv6 = (buf->base[2] == '1');
                    char addr_str_len = buf->base[3];

                    std::string addr(buf->base + 4, static_cast<std::size_t>(addr_str_len));

                    if (is_ipv4) {
                        addr = std::string("::ffff:") + addr;
                        uv_ip6_addr(addr.c_str(), HARBOUR_PORT, reinterpret_cast<sockaddr_in6*>(&another_addr));
                    } else if (is_ipv6) {
                        uv_ip6_addr(addr.c_str(), HARBOUR_PORT, reinterpret_cast<sockaddr_in6*>(&another_addr));
                    } else {
                        LOG_ERROR(SERVICE_BLUETOOTH, "Unknown connect request receipent!");
                        return;
                    }
                    requester = reinterpret_cast<const sockaddr*>(&another_addr);
                }
            } else if (buf->base[1] == 'a') {
                if (!current_active_observer_) {
                    return;
                }

                uv_timer_stop(reinterpret_cast<uv_timer_t*>(hearing_timeout_timer_));

                epoc::socket::saddress addr_for_call;
                std::memset(&addr_for_call, 0, sizeof(epoc::socket::saddress));

                internet::host_sockaddr_to_guest_saddress(requester, addr_for_call);

                if (addr_for_call.family_ == epoc::internet::INET_ADDRESS_FAMILY) {
                    epoc::internet::sinet6_address addr_for_call_temp;
                    std::memset(&addr_for_call_temp, 0, sizeof(epoc::socket::saddress));

                    addr_for_call_temp.family_ = epoc::internet::INET6_ADDRESS_FAMILY;
                    addr_for_call_temp.port_ = addr_for_call.port_;

                    std::uint32_t *addr_value_ptr = addr_for_call_temp.address_32x4();

                    std::memcpy(addr_value_ptr + 3, addr_for_call.user_data_, 4);
                    addr_value_ptr[2] = 0xFFFF0000;

                    addr_for_call = addr_for_call_temp;
                }

                const std::lock_guard<std::mutex> guard(friends_lock_);

                for (std::size_t i = 0; i < friends_.size(); i++) {
                    if (std::memcmp(&friends_[i].real_addr_, &addr_for_call, sizeof(epoc::socket::saddress)) == 0) {
                        current_active_observer_->on_stranger_call(addr_for_call, static_cast<std::uint32_t>(i));
                        reset_friend_timeout_timer(reinterpret_cast<uv_timer_t*>(hearing_timeout_timer_));

                        return;
                    }
                }

                epoc::bt::friend_info info;
                info.dvc_addr_.padding_ = 0;
                info.real_addr_ = addr_for_call;

                friends_.push_back(info);
                current_active_observer_->on_stranger_call(addr_for_call, static_cast<std::uint32_t>(friends_.size() - 1));

                reset_friend_timeout_timer(reinterpret_cast<uv_timer_t*>(hearing_timeout_timer_));
                return;
            }
        }

        uv_udp_send(send_req, reinterpret_cast<uv_udp_t*>(virt_bt_info_server_), &local_buf, 1, requester, [](uv_udp_send_t *send_info, int status) {
            delete send_info;
        });
    }
    
    void midman_inet::send_login() {
        std::string login_package("l0");
        login_package.push_back(static_cast<char>(password_.length()));
        login_package.insert(login_package.end(), password_.begin(), password_.end());

        uv_buf_t buf = uv_buf_init(login_package.data(), static_cast<std::uint32_t>(login_package.size()));
        uv_write_t *write_req = new uv_write_t;
        int err = uv_write(write_req, reinterpret_cast<uv_stream_t*>(virt_server_socket_), &buf, 1, [](uv_write_t *write_req, int status) {
            if (status < 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send login request to server! Libuv's error code {}", status);
            }
            delete write_req;
        });
        
        if (err < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send login request to server! Libuv's error code {}", err);
            delete write_req;
        }
    }

    void midman_inet::send_logout(void *hh, bool close_and_reset) {
        if (!hh) {
            return;
        }

        std::string logout_package("l1");

        uv_buf_t buf = uv_buf_init(logout_package.data(), static_cast<std::uint32_t>(logout_package.size()));

        uv_write_t *write_req = new uv_write_t;
        write_req->data = reinterpret_cast<void*>(static_cast<std::ptrdiff_t>(close_and_reset));

        int err = uv_write(write_req, reinterpret_cast<uv_stream_t*>(hh), &buf, 1, [](uv_write_t *write_req, int status) {
            if (status < 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send logout request to server! Libuv's error code {}", status);
            }

            if (write_req->data) {
                uv_tcp_close_reset(reinterpret_cast<uv_tcp_t*>(write_req->handle), [](uv_handle_t *hh) {
                    delete hh;
                });
            }

            delete write_req;
        });
        
        if (err < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send logout request to server! Libuv's error code {}", err);
            delete write_req;

            if (close_and_reset) {
                uv_tcp_close_reset(reinterpret_cast<uv_tcp_t*>(hh), [](uv_handle_t *hh) {
                    delete hh;
                });
            }
        }
    }

    void midman_inet::handle_meta_server_msg(std::int64_t nread, const uv_buf_t *buf_ptr) {
        if ((nread == -1) || (nread == 0) || (nread == UV_EOF)) {
            uv_read_stop(reinterpret_cast<uv_stream_t*>(virt_server_socket_));
            uv_tcp_close_reset(reinterpret_cast<uv_tcp_t*>(virt_server_socket_), [](uv_handle_t *hh) {
                uv_tcp_t *tt = reinterpret_cast<uv_tcp_t*>(hh);
                delete tt;
            });

            virt_server_socket_ = nullptr;
            return;
        }

        // Everything else is through UDP
    }

    bool midman_inet::get_friend_address(const device_address &friend_addr, epoc::socket::saddress &addr) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return false;
        }

        const std::lock_guard<std::mutex> guard(friends_lock_);

lookup:
        for (std::size_t i = 0; i < friends_.size(); i++) {
            if (std::memcmp(friend_addr.addr_, friends_[i].dvc_addr_.addr_, 6) == 0) {
                addr = friends_[i].real_addr_;
                return true;
            }
        }

        if (!friend_info_cached_) {
            // Try to refresh the local cache. It's not really ideal, but anyway resolver always redo
            // a full rescan...
            refresh_friend_infos();
            goto lookup;
        }

        return false;
    }

    bool midman_inet::get_friend_address(const std::uint32_t index, epoc::socket::saddress &addr) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return false;
        }
        const std::lock_guard<std::mutex> guard(friends_lock_);
        if ((index >= friends_.size()) && (friends_[index].real_addr_.family_ == 0)) {
            return false;
        }

        addr = friends_[index].real_addr_;
        return true;
    }

    void midman_inet::add_device_address_mapping(const std::uint32_t index, const device_address &addr) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        const std::lock_guard<std::mutex> guard(friends_lock_);

        if ((friends_[index].dvc_addr_.padding_ == 1) || (std::memcmp(friends_[index].dvc_addr_.addr_, addr.addr_, 6) != 0)) {
            friends_[index].dvc_addr_ = addr;
            friends_[index].dvc_addr_.padding_ = 0;
        }
    }

    void midman_inet::refresh_friend_infos() {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if (friend_info_cached_) {
            return;
        }

        common::event done_event;

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(friends_.size()); i++) {
            if (friends_[i].real_addr_.family_ != 0) {
                done_event.reset();
                device_addr_asker_.send_request_with_retries(friends_[i].real_addr_, "a", 1, [this, i, &done_event](const char *result, const ssize_t bytes) {
                    if (bytes > 0) {
                        std::memcpy(&friends_[i].dvc_addr_, result, sizeof(device_address));
                        friend_device_address_mapping_.emplace(friends_[i].dvc_addr_, i);
                    }
                    done_event.set();
                });
                done_event.wait();
            }
        }

        friend_info_cached_ = true;
    }

    std::uint32_t midman_inet::lookup_host_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return 0;
        }

        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            return 0;
        }
        
        if (allocated_ports_.is_allocated(virtual_port - 1)) {
            return port_offset_ + virtual_port - 1;
        }

        return 0;
    }

    void midman_inet::add_host_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }

        if (should_upnp_apply_to_port()) {
            UPnP::TryPortmapping(virtual_port - 1 + port_offset_, false);
        }

        allocated_ports_.force_fill(virtual_port - 1, 1);
        port_refs_[virtual_port - 1] = 1;
    }
    
    std::uint16_t midman_inet::get_free_port() {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return 0;
        }

        // Reserve first 10 ports for system
        int size = 1;
        const int offset = allocated_ports_.allocate_from(10, size, false);
        if (offset < 0) {
            return 0;
        }

        return static_cast<std::uint16_t>(offset + 1);
    }

    void midman_inet::ref_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }
        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }
        
        port_refs_[virtual_port - 1]++;
    }

    void midman_inet::close_port(const std::uint16_t virtual_port) {
        if (discovery_mode_ == DISCOVERY_MODE_OFF) {
            return;
        }

        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }

        if (allocated_ports_.is_allocated(virtual_port - 1)) {
            std::uint32_t ref_count = --port_refs_[virtual_port - 1];
            if (ref_count <= 0) {
                if (should_upnp_apply_to_port()) {
                    UPnP::StopPortmapping(virtual_port, false);
                }
                allocated_ports_.deallocate(virtual_port - 1, 1);
            }
        }
    }

    void midman_inet::update_friend_list(const std::vector<config::friend_address> &addrs, std::vector<std::uint64_t> &invalid_address_indicies) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        if (discovery_mode_ != DISCOVERY_MODE_DIRECT_IP) {
            return;
        }

        sockaddr_in6 in_temp;
        int res = 0;

        epoc::bt::friend_info info;
        info.dvc_addr_.padding_ = 1;

        for (std::size_t i = 0; i < common::min<std::size_t>(addrs.size(), MAX_INET_DEVICE_AROUND); i++) {
            // Max TCP port number
            if (addrs[i].port_ > 65535) {
                invalid_address_indicies.push_back(FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER | static_cast<std::uint32_t>(i));
            } else {
                std::string ip_str = addrs[i].addr_;
                if (addrs[i].addr_.find(':') == std::string::npos) {
                    ip_str = std::string("::ffff:") + ip_str;
                }

                res = uv_ip6_addr(ip_str.data(), addrs[i].port_, &in_temp);

                if (res != 0) {
                    invalid_address_indicies.push_back(FRIEND_UPDATE_ERROR_INVALID_ADDR | static_cast<std::uint32_t>(i));
                } else {
                    std::memset(&info.real_addr_, 0, sizeof(epoc::socket::saddress));
                    internet::host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr*>(&in_temp), info.real_addr_);
                
                    friends_.push_back(info);
                }
            }
        }
    }

    std::vector<std::uint32_t> midman_inet::get_friend_index_with_address(epoc::socket::saddress &addr) {
        bool is_ipv4 = (addr.family_ == epoc::internet::INET_ADDRESS_FAMILY);
        std::vector<std::uint32_t> indicies;

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(friends_.size()); i++) {
            if (friends_[i].real_addr_.family_ == 0) {
                continue;
            }

            bool is_current_ipv4 = (friends_[i].real_addr_.family_ == epoc::internet::INET_ADDRESS_FAMILY);
            if (is_current_ipv4 != is_ipv4) {
                continue;
            }

            if (is_ipv4 && (*static_cast<epoc::internet::sinet_address&>(friends_[i].real_addr_).addr_long() == 
                *(static_cast<epoc::internet::sinet_address&>(addr).addr_long()))) {
                indicies.push_back(i);
            } else if (!is_ipv4 && (std::memcmp(static_cast<epoc::internet::sinet6_address&>(friends_[i].real_addr_).address_32x4(),
                static_cast<epoc::internet::sinet6_address&>(addr).address_32x4(), sizeof(std::uint32_t) * 4) == 0)) {
                indicies.push_back(i);
            }
        }

        return indicies;
    }

    bool midman_inet::get_friend_device_address(const std::uint32_t index, device_address &result) {
        if (index >= friends_.size()) {
            return false;
        }

        if (!friend_info_cached_) {
            // Try to refresh the local cache. It's not really ideal, but anyway resolver always redo
            // a full rescan...
            refresh_friend_infos();
        }

        if (friends_[index].real_addr_.family_ == 0) {
            return false;
        }

        result = friends_[index].dvc_addr_;
        return true;
    }

    void midman_inet::add_or_update_friend(const epoc::socket::saddress &addr) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        for (std::size_t i = 0; i < friends_.size(); i++) {
            if (memcmp(&(friends_[i].real_addr_), &addr, sizeof(epoc::socket::saddress)) == 0) {
                friends_[i].dvc_addr_.padding_ = 1;
                return;
            }
        }

        friend_info info;
        info.real_addr_ = addr;
        info.dvc_addr_.padding_ = 1;

        friends_.push_back(info);
    }

    void midman_inet::send_call_for_strangers() {
        uv_udp_send_t *send_req = new uv_udp_send_t;
        
        struct call_stranger_request_data {
            midman_inet *self_;
            std::vector<char> call_request_;
            sockaddr_in6 server_addr_;
        };

        call_stranger_request_data *request_data = new call_stranger_request_data;
        request_data->self_ = this;

        request_data->call_request_.push_back('c');
        request_data->call_request_.push_back('r');

        if (discovery_mode_ == DISCOVERY_MODE_LOCAL_LAN) {
            request_data->call_request_.push_back(static_cast<char>(password_.length()));
            request_data->call_request_.insert(request_data->call_request_.end(), password_.begin(), password_.end());
        }

        sockaddr *addr_ptr = nullptr;
        GUEST_TO_BSD_ADDR(server_addr_, addr_ptr);

        // A bit of overflow would be ok, I guess))
        std::memcpy(&request_data->server_addr_, addr_ptr, sizeof(sockaddr_in6));
        request_data->server_addr_.sin6_port = htons(port_);

        send_req->data = request_data;

        uv_async_t *async = new uv_async_t;
        async->data = send_req;

        uv_async_init(uv_default_loop(), async, [](uv_async_t *async) {
            uv_udp_send_t *send_obj = reinterpret_cast<uv_udp_send_t*>(async->data);
            call_stranger_request_data *request_data = reinterpret_cast<call_stranger_request_data*>(send_obj->data);
            uv_udp_t *udp_handle = reinterpret_cast<uv_udp_t*>(request_data->self_->virt_bt_info_server_);
            uv_timer_t *timer = reinterpret_cast<uv_timer_t*>(request_data->self_->hearing_timeout_timer_);

            midman_inet *mid = request_data->self_;
            timer->data = mid;

            uv_buf_t local_buf = uv_buf_init(request_data->call_request_.data(), static_cast<unsigned int>(request_data->call_request_.size()));

            if (mid->get_discovery_mode() != DISCOVERY_MODE_LOCAL_LAN) {
                uv_write_t *write_req = new uv_write_t;
                write_req->data = send_obj->data;

                delete send_obj;

                int sts = uv_write(write_req, reinterpret_cast<uv_stream_t*>(mid->virt_server_socket_), &local_buf, 1, [](uv_write_t *write, int status) {
                    call_stranger_request_data *request_data = reinterpret_cast<call_stranger_request_data*>(write->data);
                    delete request_data;

                    if (status < 0) {
                        LOG_ERROR(SERVICE_BLUETOOTH, "Discovery request send to server failed with libuv's code {}", status);
                    }

                    delete write;
                });

                if (sts < 0) {
                    LOG_ERROR(SERVICE_BLUETOOTH, "Discovery request send to server failed with libuv's code {}", sts);
                    delete write_req;
                }
            } else {
                uv_udp_set_broadcast(udp_handle, 1);
                uv_udp_send(send_obj, udp_handle, &local_buf, 1, reinterpret_cast<const sockaddr*>(&request_data->server_addr_), [](uv_udp_send_t *send_info, int status) {
                    call_stranger_request_data *request_data = reinterpret_cast<call_stranger_request_data*>(send_info->data);

                    delete request_data;
                    delete send_info;
                });
            }

            reset_friend_timeout_timer(timer);

            uv_close(reinterpret_cast<uv_handle_t*>(async), [](uv_handle_t *handle) {
                uv_async_t *async = reinterpret_cast<uv_async_t*>(handle);
                delete async;
            });
        });

        uv_async_send(async);
    }

    void midman_inet::begin_hearing_stranger_call(inet_stranger_call_observer *observer) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        if (current_active_observer_ || !pending_observers_.empty()) {
            pending_observers_.push_back(observer);
        } else {
            current_active_observer_ = observer;
            send_call_for_strangers();
        }
    }

    void midman_inet::unregister_stranger_call_observer(inet_stranger_call_observer *observer) {
        const std::lock_guard<std::mutex> guard(friends_lock_);
        if (observer == current_active_observer_) {
            current_active_observer_ = nullptr;
            return;
        }

        auto ite = std::find(pending_observers_.begin(), pending_observers_.end(), observer);
        if (ite == pending_observers_.end()) {
            return;
        }

        pending_observers_.erase(ite);
    }
}
