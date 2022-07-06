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

namespace eka2l1::epoc::bt {
    btinet_socket::btinet_socket(btlink_inet_protocol *protocol, std::unique_ptr<epoc::socket::socket> &inet_socket)
        : inet_socket_(std::move(inet_socket))
        , scan_value_(HCI_INQUIRY_AND_PAGE_SCAN)
        , protocol_(protocol)
        , virtual_port_(0) {

    }
 
    btinet_socket::~btinet_socket() {
        if (virtual_port_ != 0) {
            midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());
            midman->close_port(virtual_port_);
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

            midman->add_host_port(guest_port, addr_to_bind.port_);
        } else {
            addr_to_bind.port_ = host_port;
            inet_socket_->bind(addr_to_bind, info);
        }

        virtual_port_ = guest_port;
    }

    std::int32_t btinet_socket::local_name(epoc::socket::saddress &result, std::uint32_t &result_len) {
        socket_device_address &addr = static_cast<socket_device_address&>(result);
        device_address *addr_dvc = addr.get_device_address();

        addr_dvc->addr_[0] = 'E';
        addr_dvc->addr_[1] = 'K';
        addr_dvc->addr_[2] = 'A';
        addr_dvc->addr_[3] = '2';
        addr_dvc->addr_[4] = 'L';
        addr_dvc->addr_[5] = '0';
        addr.port_ = virtual_port_;

        result_len = socket_device_address::DATA_LEN;

        return epoc::error_none;
    }

    std::int32_t btinet_socket::listen(const std::uint32_t backlog) {
        return inet_socket_->listen(backlog);
    }

    void btinet_socket::accept(std::unique_ptr<epoc::socket::socket> *pending_sock, epoc::notify_info &complete_info) {
        inet_socket_->accept(pending_sock, complete_info);
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