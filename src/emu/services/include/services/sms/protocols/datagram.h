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

#pragma once

#include <services/sms/protocols/common.h>
#include <services/socket/protocol.h>

#include <atomic>

namespace eka2l1::epoc::sms {
    class datagram_protocol;

    class datagram_socket : public socket::socket {
    private:
        datagram_protocol *pr_;

    public:
        explicit datagram_socket(datagram_protocol *pr)
            : pr_(pr) {
        }

        ~datagram_socket() override;

        std::int32_t bind(const std::uint8_t *sockaddr_buffer, const std::size_t available_size) override;

        void ioctl(const std::uint32_t command, epoc::notify_info &complete_info, std::uint8_t *buffer,
            const std::size_t available_size, const std::size_t max_buffer_size, const std::uint32_t level) override;
        
        void send(const std::uint8_t *data, const std::size_t data_size, std::size_t *sent_size, const std::uint8_t *sockaddr_buffer,
            const std::size_t sockaddr_buffer_size, const std::uint32_t flags, epoc::notify_info &complete_info) override;
    };

    class datagram_protocol : public socket::protocol {
    protected:
        std::atomic<std::uint32_t> socket_count_;
        friend class datagram_socket;

    public:
        explicit datagram_protocol(const bool oldarch)
            : socket::protocol(oldarch)
            , socket_count_(0) {
        }

        virtual std::u16string name() const override {
            return u"SMS Datagram";
        }

        std::uint32_t family_id() const override {
            return SMS_ADDRESS_FAMILY;
        }

        virtual std::uint32_t id() const override {
            return SMS_DTG_PROTOCOL;
        }

        virtual epoc::version ver() const override {
            epoc::version v;
            v.major = 0;
            v.minor = 0;
            v.build = 1;

            return v;
        }

        virtual epoc::socket::byte_order get_byte_order() const override {
            return epoc::socket::byte_order_little_endian;
        }

        virtual std::unique_ptr<epoc::socket::host_resolver> make_host_resolver() override {
            return nullptr;
        }
        
        virtual std::unique_ptr<epoc::socket::socket> make_socket() override;
    };
}