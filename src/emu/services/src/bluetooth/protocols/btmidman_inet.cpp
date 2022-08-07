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
#include <common/random.h>
#include <common/log.h>
#include <common/algorithm.h>
#include <config/config.h>

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::bt {
    midman_inet::midman_inet(const config::state &conf)
        : midman()
        , allocated_ports_(MAX_PORT)
        , friend_info_cached_(false)
        , virt_bt_info_server_(nullptr)
        , port_(conf.internet_bluetooth_port) {
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

        virt_bt_info_server_detail->data = this;

        sockaddr_in6 addr_bind;
        std::memset(&addr_bind, 0, sizeof(sockaddr_in6));
        addr_bind.sin6_family = AF_INET6;
        addr_bind.sin6_port = htons(static_cast<std::uint16_t>(port_));

        uv_udp_bind(virt_bt_info_server_detail, reinterpret_cast<const sockaddr*>(&addr_bind), 0);
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
        uv_udp_recv_stop(reinterpret_cast<uv_udp_t*>(virt_bt_info_server_));
        uv_close(reinterpret_cast<uv_handle_t*>(virt_bt_info_server_), [](uv_handle_t *hh) {
            delete hh;
        });
    }

    void midman_inet::prepare_server_recv_buffer(void *buf_void, const std::size_t suggested_size) {
        uv_buf_t *buf = reinterpret_cast<uv_buf_t*>(buf_void);
        server_recv_buf_.resize(suggested_size);
        
        buf->base = server_recv_buf_.data();
        buf->len = static_cast<std::uint32_t>(suggested_size);
    }

    void midman_inet::handle_server_request(const sockaddr *requester, const void *buf_void, std::int64_t nread) {
        const uv_buf_t *buf = reinterpret_cast<const uv_buf_t*>(buf_void);

        uv_udp_send_t *send_req = new uv_udp_send_t;
        uv_buf_t local_buf;
        std::uint32_t temp_uint;
        char temp_char;
        
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

                for (auto &[virt_port, port]: port_map_) {
                    if (static_cast<std::uint32_t>(port & 0xFFFFFFFF) == requested_port) {
                        temp_char = '1';
                        break;
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
        }

        uv_udp_send(send_req, reinterpret_cast<uv_udp_t*>(virt_bt_info_server_), &local_buf, 1, requester, [](uv_udp_send_t *send_info, int status) {
            delete send_info;
        });
    }

    bool midman_inet::get_friend_address(const device_address &friend_addr, epoc::socket::saddress &addr) {
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
        const std::lock_guard<std::mutex> guard(friends_lock_);
        if ((index >= friends_.size()) && (friends_[index].real_addr_.family_ == 0)) {
            return false;
        }

        addr = friends_[index].real_addr_;
        return true;
    }

    void midman_inet::add_device_address_mapping(const std::uint32_t index, const device_address &addr) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        if ((friends_[index].dvc_addr_.padding_ == 1) || (std::memcmp(friends_[index].dvc_addr_.addr_, addr.addr_, 6) != 0)) {
            friends_[index].dvc_addr_ = addr;
            friends_[index].dvc_addr_.padding_ = 0;
        }
    }

    void midman_inet::refresh_friend_infos() {
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
        auto result = port_map_.find(virtual_port);
        if (result == port_map_.end()) {
            return 0;
        }

        return static_cast<std::uint32_t>(result->second & 0xFFFFFFFF);
    }

    void midman_inet::add_host_port(const std::uint16_t virtual_port, const std::uint32_t host_port) {
        if ((virtual_port > MAX_PORT) || (virtual_port == 0)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} is out of allowed range!", virtual_port);
            return;
        }

        if (port_map_.find(virtual_port) != port_map_.end()) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Port {} already mapped to host!", virtual_port);
            return;
        }

        port_map_[virtual_port] = (1ULL << 32) | host_port;
        allocated_ports_.force_fill(virtual_port - 1, 1);
    }
    
    std::uint16_t midman_inet::get_free_port() {
        // Reserve first 10 ports for system
        int size = 1;
        const int offset = allocated_ports_.allocate_from(10, size, false);
        if (offset < 0) {
            return 0;
        }

        return static_cast<std::uint16_t>(offset + 1);
    }

    void midman_inet::ref_port(const std::uint16_t virtual_port) {
        std::uint64_t val = port_map_[virtual_port];
        std::uint32_t ref_count = static_cast<std::uint32_t>(val >> 32);

        port_map_[virtual_port] = ((static_cast<std::uint64_t>(ref_count + 1) << 32) | (val & 0xFFFFFFFF));
    }

    void midman_inet::close_port(const std::uint16_t virtual_port) {
        auto ite = port_map_.find(virtual_port);
        if (ite != port_map_.end()) {
            std::uint32_t ref_count = static_cast<std::uint32_t>(ite->second >> 32);
            if (ref_count > 1) {
                ite->second = ((static_cast<std::uint64_t>(ref_count - 1) << 32) | (ite->second & 0xFFFFFFFF));
                return;
            } else {
                port_map_.erase(ite);
            }
        } 
        allocated_ports_.deallocate(virtual_port - 1, 1);
    }

    void midman_inet::update_friend_list(const std::vector<config::friend_address> &addrs, std::vector<std::uint64_t> &invalid_address_indicies) {
        const std::lock_guard<std::mutex> guard(friends_lock_);

        std::uint32_t current_friend = 0;
        sockaddr_in6 in_temp;
        int res = 0;

        for (std::size_t i = 0; i < friends_.size(); i++) {
            friends_[i].real_addr_.family_ = 0;
            friends_[i].dvc_addr_.padding_ = 1;
        }

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
                    internet::host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr*>(&in_temp), friends_[current_friend++].real_addr_);
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
}
