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

#pragma once

#include <services/bluetooth/protocols/common.h>
#include <services/bluetooth/protocols/asker_inet.h>
#include <services/socket/socket.h>
#include <common/sync.h>

#include <memory>

namespace eka2l1::epoc::bt {
    class midman_inet;
    class btlink_inet_protocol;
 
    struct btinet_socket: public socket::socket {
    private:
        hci_scan_enable_ioctl_val scan_value_;

        std::unique_ptr<epoc::socket::socket> inet_socket_;
        asker_inet info_asker_;

    protected:
        btlink_inet_protocol *protocol_;

        std::uint16_t virtual_port_;
        socket_device_address remote_addr_;
        bool remote_calculated_;

        common::event port_exist_ask_done_event_;

    public:
        explicit btinet_socket(btlink_inet_protocol *protocol, std::unique_ptr<epoc::socket::socket> &inet_socket);
        virtual ~btinet_socket() override;

        void bind(const epoc::socket::saddress &addr, epoc::notify_info &info) override;
        void accept(std::unique_ptr<epoc::socket::socket> *pending_sock, epoc::notify_info &complete_info) override;
        void send(const std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, const epoc::socket::saddress *addr, std::uint32_t flags, epoc::notify_info &complete_info) override;
        void receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, epoc::socket::saddress *addr,
            std::uint32_t flags, epoc::notify_info &complete_info, epoc::socket::receive_done_callback done_callback) override;
        void connect(const epoc::socket::saddress &addr, epoc::notify_info &info) override;
        void shutdown(epoc::notify_info &complete_info, int reason) override;

        void cancel_receive() override;
        void cancel_send() override;
        void cancel_connect() override;
        void cancel_accept() override;

        std::int32_t listen(const std::uint32_t backlog) override;
        std::int32_t local_name(epoc::socket::saddress &result, std::uint32_t &result_len) override;
        std::int32_t remote_name(epoc::socket::saddress &result, std::uint32_t &result_len) override;
 
        void ioctl(const std::uint32_t command, epoc::notify_info &complete_info, std::uint8_t *buffer,
            const std::size_t available_size, const std::size_t max_buffer_size, const std::uint32_t level) override;
    };
}