/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/sms/protocols/datagram.h>
#include <common/log.h>
#include <utils/err.h>

namespace eka2l1::epoc::sms {
    std::unique_ptr<epoc::socket::socket> datagram_protocol::make_socket() {
        if (socket_count_ >= SMS_MAX_SOCKS) {
            LOG_ERROR(SERVICE_SMS, "Number of SMS socket instances exceed maximum!");
            return nullptr;
        }

        socket_count_++;
        return std::make_unique<datagram_socket>(this);
    }

    datagram_socket::~datagram_socket() {
        if (pr_) {
            pr_->socket_count_--;
        }
    }

    std::int32_t datagram_socket::bind(const std::uint8_t *sockaddr_buffer, const std::size_t available_size) {
        LOG_INFO(SERVICE_SMS, "Socket bind stubbed to success");
        return epoc::error_none;
    }

    void datagram_socket::ioctl(const std::uint32_t command, epoc::notify_info &complete_info, std::uint8_t *buffer,
        const std::size_t available_size, const std::size_t max_buffer_size, const std::uint32_t level) {
        LOG_INFO(SERVICE_SMS, "Socket IOCTL stubbed with complete (opcode={})", command);
        complete_info.complete(epoc::error_none);
    }
    
    void datagram_socket::send(const std::uint8_t *data, const std::size_t data_size, std::size_t *sent_size, const std::uint8_t *sockaddr_buffer,
        const std::size_t sockaddr_buffer_size, const std::uint32_t flags, epoc::notify_info &complete_info) {
        //LOG_INFO(SERVICE_SMS, "Socket send stubbed with complete!");
        complete_info.complete(epoc::error_none);
    }
}