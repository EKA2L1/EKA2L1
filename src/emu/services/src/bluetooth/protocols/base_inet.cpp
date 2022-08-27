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

#include <services/bluetooth/protocols/base_inet.h>
#include <services/bluetooth/protocols/btlink/btlink_inet.h>
#include <services/internet/protocols/inet.h>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <utils/err.h>

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::bt {
    btinet_socket::btinet_socket(btlink_inet_protocol *protocol, std::unique_ptr<epoc::socket::socket> &inet_socket)
        : inet_socket_(std::move(inet_socket))
        , scan_value_(HCI_INQUIRY_AND_PAGE_SCAN)
        , protocol_(protocol)
        , virtual_port_(0)
        , remote_calculated_(false) {
        midman_inet *mid = reinterpret_cast<midman_inet*>(protocol_->get_midman());
        if (inet_socket_) {
            int opt_value = 1;
            inet_socket_->set_option(internet::INET_TCP_NO_DELAY_OPT, internet::INET_TCP_SOCK_OPT_LEVEL, reinterpret_cast<std::uint8_t*>(&opt_value), 4);
        }
        if (inet_socket_ && (mid->get_discovery_mode() != DISCOVERY_MODE_DIRECT_IP)) {
            reinterpret_cast<epoc::internet::inet_socket*>(inet_socket_.get())->set_socket_accepted_hook([this](void *opaque_handle) {
                midman_inet *mid = reinterpret_cast<midman_inet*>(protocol_->get_midman());

                sockaddr_in6 addr;
                int addr_structlen = sizeof(sockaddr_in6);

                epoc::socket::saddress addr_dest;
                std::memset(&addr_dest, 0, sizeof(epoc::socket::saddress));

                uv_tcp_getpeername(reinterpret_cast<uv_tcp_t*>(opaque_handle), reinterpret_cast<sockaddr*>(&addr), &addr_structlen);                
                uv_tcp_nodelay(reinterpret_cast<uv_tcp_t*>(opaque_handle), 1);

                epoc::internet::host_sockaddr_to_guest_saddress(reinterpret_cast<const sockaddr*>(&addr), addr_dest);

                addr_dest.port_ = static_cast<std::uint16_t>(mid->get_server_port());

                mid->add_or_update_friend(addr_dest);
                mid->clear_friend_info_cached();
            });
        }
    }
 
    btinet_socket::~btinet_socket() {
        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());

        if (virtual_port_ != 0) {
            midman->close_port(virtual_port_);
        }

        if (inet_socket_ && (midman->get_discovery_mode() != DISCOVERY_MODE_DIRECT_IP)) {
            reinterpret_cast<epoc::internet::inet_socket*>(inet_socket_.get())->set_socket_accepted_hook(nullptr);
        }
    }

    void btinet_socket::bind(const epoc::socket::saddress &addr, epoc::notify_info &info) {
        if (addr.port_ > midman::MAX_PORT) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Bluetooth bind port is only allowed to be from 1 to 60!");
            info.complete(epoc::error_argument);

            return;
        }

        epoc::socket::saddress addr_to_bind = addr;
        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());
  
        if (addr.family_ != BTADDR_PROTOCOL_FAMILY_ID) {
            // If 0, they just want to change the port. Well, acceptable
            if (addr.family_ != 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Address to bind is not a Bluetooth family address!");
                info.complete(epoc::error_argument);
                return;
            }
        }

        std::memset(addr_to_bind.user_data_, 0, sizeof(addr_to_bind.user_data_));
        addr_to_bind.family_ = internet::INET6_ADDRESS_FAMILY;

        std::uint32_t result_len;
        std::uint16_t guest_port = addr_to_bind.port_;

        if (guest_port == 0) {
            guest_port = midman->get_free_port();
            if (guest_port == 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Bluetooth ports have ran out. Can't bind!");
                info.complete(epoc::error_eof);

                return;
            }
        }

        std::uint32_t host_port = midman->lookup_host_port(guest_port);
        if (host_port == 0) {
            addr_to_bind.port_ = 0;

            inet_socket_->bind(addr_to_bind, info);
            inet_socket_->local_name(addr_to_bind, result_len);

            midman->add_host_port(guest_port);
        } else {
            addr_to_bind.port_ = host_port;
            inet_socket_->bind(addr_to_bind, info);
        }

        virtual_port_ = guest_port;
    }

    void btinet_socket::connect(const epoc::socket::saddress &addr, epoc::notify_info &info) {
        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());
  
        if (addr.family_ != BTADDR_PROTOCOL_FAMILY_ID) {
            // If 0, they just want to change the port. Well, acceptable
            LOG_ERROR(SERVICE_BLUETOOTH, "Address to bind is not a Bluetooth family address!");
            info.complete(epoc::error_argument);
            return;
        }

        const socket_device_address &dvc_addr = static_cast<const socket_device_address&>(addr);
        epoc::socket::saddress real_addr;

        // NOTE: Need real address actually T_T
        if (!midman->get_friend_address(*dvc_addr.get_device_address_const(), real_addr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Can't retrieve real INET address!");
            info.complete(epoc::error_could_not_connect);
            return;
        }

        // There's no mechanism exposed to client to know whether it's correct, just an absolute fact that it's
        // gonna be a valid value when the socket is connected. Set it here won't hurt
        remote_addr_ = dvc_addr;
        remote_addr_.family_ = BTADDR_PROTOCOL_FAMILY_ID;

        remote_calculated_ = true;

        info_asker_.ask_for_routed_port_async(addr.port_, real_addr, [info, real_addr, this](const std::int64_t res) {
            epoc::notify_info info_copy = info;

            if (res <= 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Retrieve real port of Virtual bluetooth address failed with error code {}", res);
                info_copy.complete(epoc::error_could_not_connect);
            } else {
                epoc::socket::saddress addr_copy = real_addr;
                addr_copy.port_ = static_cast<std::uint32_t>(res);

                inet_socket_->connect(addr_copy, info_copy);
            }
        });
    }

    std::int32_t btinet_socket::local_name(epoc::socket::saddress &result, std::uint32_t &result_len) {
        socket_device_address &addr = static_cast<socket_device_address&>(result);

        device_address *addr_dvc = addr.get_device_address();
        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());

        *addr_dvc = midman->local_device_address();

        addr.family_ = BTADDR_PROTOCOL_FAMILY_ID;
        addr.port_ = virtual_port_;

        result_len = socket_device_address::DATA_LEN;

        return epoc::error_none;
    }

    std::int32_t btinet_socket::remote_name(epoc::socket::saddress &result, std::uint32_t &result_len) {
        if (!remote_calculated_) {
            epoc::socket::saddress addr_real;
            std::uint32_t len_temp;

            const std::int32_t res = inet_socket_->remote_name(addr_real, len_temp);
            if (res != epoc::error_none) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Getting bluetooth's remote name failed with code {}!", res);
                return res;
            }
            
            midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());
            std::vector<std::uint32_t> list_indicies = midman->get_friend_index_with_address(addr_real);

            std::uint32_t correct_index = 0;

            if (list_indicies.size() > 1) {
                correct_index = static_cast<std::uint32_t>(-1);
                std::uint32_t correct_port = addr_real.port_;
                
                std::vector<char> request_data;
                request_data.push_back('p');
                request_data.push_back('e');
                request_data.push_back(static_cast<char>(correct_port));
                request_data.push_back(static_cast<char>(correct_port >> 8));
                request_data.push_back(static_cast<char>(correct_port >> 16));
                request_data.push_back(static_cast<char>(correct_port >> 24));

                for (std::size_t i = 0; i < list_indicies.size(); i++) {
                    midman->get_friend_address(list_indicies[i], addr_real);

                    port_exist_ask_done_event_.reset();
                    bool have_it = false;

                    info_asker_.send_request_with_retries(addr_real, request_data.data(), request_data.size(), [this, &have_it](const char *result, const ssize_t bytes) {
                        if (bytes >= 1) {
                            have_it = (result[0] == '1');
                        }
                        port_exist_ask_done_event_.set();
                    });

                    port_exist_ask_done_event_.wait();

                    if (have_it) {
                        correct_index = static_cast<std::uint32_t>(i);
                        break;
                    }
                }

                if (correct_index == static_cast<std::uint32_t>(-1)) {
                    LOG_ERROR(SERVICE_BLUETOOTH, "Can't find which friend address connected to socket (too many to choose!)");
                    return epoc::error_general;
                }
            }
        
            if (!midman->get_friend_device_address(correct_index, *remote_addr_.get_device_address())) {
                LOG_WARN(SERVICE_BLUETOOTH, "Failed to get bluetooth device address from friend index {}", list_indicies[0]);
                return epoc::error_could_not_connect;
            }

            remote_addr_.family_ = BTADDR_PROTOCOL_FAMILY_ID;
            remote_calculated_ = true;
        }

        static_cast<socket_device_address&>(result) = remote_addr_;
        result_len = socket_device_address::DATA_LEN;

        return epoc::error_none;
    }

    std::int32_t btinet_socket::listen(const std::uint32_t backlog) {
        return inet_socket_->listen(backlog);
    }

    void btinet_socket::accept(std::unique_ptr<epoc::socket::socket> *pending_sock, epoc::notify_info &complete_info) {
        *pending_sock = protocol_->make_empty_base_link_socket();

        btinet_socket *casted_pending_sock_ptr = reinterpret_cast<btinet_socket*>(pending_sock->get());
        casted_pending_sock_ptr->virtual_port_ = virtual_port_;

        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());
        midman->ref_port(virtual_port_);

        inet_socket_->accept(&casted_pending_sock_ptr->inet_socket_, complete_info);
    }

    void btinet_socket::send(const std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, const epoc::socket::saddress *addr, std::uint32_t flags, epoc::notify_info &complete_info) {
        inet_socket_->send(data, data_size, sent_size, addr, flags, complete_info);
    }

    void btinet_socket::receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *recv_size, epoc::socket::saddress *addr,
        std::uint32_t flags, epoc::notify_info &complete_info, epoc::socket::receive_done_callback done_callback) {
        // TODO: Hook back in case of receiving address (convert it to virtual BT addr)
        inet_socket_->receive(data, data_size, recv_size, addr, flags, complete_info, done_callback);
    }

    void btinet_socket::shutdown(epoc::notify_info &complete_info, int reason) {
        inet_socket_->shutdown(complete_info, reason);
    }

    void btinet_socket::cancel_receive() {
        inet_socket_->cancel_receive();
    }

    void btinet_socket::cancel_send() {
        inet_socket_->cancel_send();
    }

    void btinet_socket::cancel_connect() {
        inet_socket_->cancel_connect();
    }

    void btinet_socket::cancel_accept() {
        inet_socket_->cancel_accept();
    }

    void btinet_socket::ioctl(const std::uint32_t command, epoc::notify_info &complete_info, std::uint8_t *buffer,
        const std::size_t available_size, const std::size_t max_buffer_size, const std::uint32_t level) {
        if (level == SOL_BT_HCI) {
            switch (command) {
            // Set the scan mode of bluetooth, either inquiry (need access code) or scan (you no need IAC)
            // Does not matter in INET mode
            case HCI_WRITE_SCAN_ENABLE:
                scan_value_ = *reinterpret_cast<hci_scan_enable_ioctl_val*>(buffer);
                complete_info.complete(epoc::error_none);
                return;

            case HCI_READ_SCAN_ENABLE:
                *reinterpret_cast<hci_scan_enable_ioctl_val*>(buffer) = scan_value_;
                complete_info.complete(epoc::error_none);
                return;

            case HCI_WRITE_DISCOVERABLITY:
            case HCI_READ_DISCOVERABILITY:
                complete_info.complete(epoc::error_none);
                return;

            case HCI_LOCAL_ADDRESS: {
                midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());

                device_address *addr = reinterpret_cast<device_address*>(buffer);
                *addr = midman->local_device_address();

                complete_info.complete(epoc::error_none);
                return;
            }

            default:
                break;
            }
        } else if (level == SOL_BT_LINK_MANAGER) {
            switch (command) {
            case LINK_MANAGER_DISCONNECT_ALL_ACL_CONNECTIONS:
                // This works at a low level, disconnect all connections in a port so that the client can establish a non-interfered connection.
                LOG_WARN(SERVICE_BLUETOOTH, "Disconnecting all ACL at specfic address not yet implemented! Stubbing it.");
                complete_info.complete(epoc::error_none);
                return;

            default:
                break;
            }
        }

        LOG_WARN(SERVICE_BLUETOOTH, "Unhandled IOCTL command for base bluetooth socket (group={}, command={})", level, command);
    }
}
