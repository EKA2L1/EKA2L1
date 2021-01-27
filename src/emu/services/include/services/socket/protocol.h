/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/socket/host.h>
#include <services/socket/socket.h>

#include <utils/version.h>

#include <cstdint>
#include <memory>

namespace eka2l1::epoc::socket {
    enum byte_order {
        byte_order_little_endian = 0,
        byte_order_big_endian = 1,
        byte_order_others = 2
    };

    struct protocol {
    private:
        enum {
            FLAG_OLDARCH = 1 << 0
        };

        std::uint32_t flags_;

    public:
        explicit protocol(const bool oldarch);

        const bool is_oldarch() const {
            return flags_ & FLAG_OLDARCH;
        }

        virtual std::u16string name() const = 0;

        /// Get the family identifier of the protocol.
        virtual std::uint32_t family_id() const = 0;

        /// Get ID of the protocol in the family
        virtual std::uint32_t id() const = 0;
        virtual epoc::version ver() const = 0;
        virtual byte_order get_byte_order() const = 0;

        virtual std::unique_ptr<host_resolver> make_host_resolver() = 0;
        virtual std::unique_ptr<socket> make_socket() = 0;
    };
};