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

#include <services/socket/common.h>
#include <utils/des.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::epoc::socket {
    struct protocol;

#pragma pack(push, 1)
    struct name_entry {
        enum {
            FLAG_ALIAS_NAME = 1 << 0,
            FLAG_PARTIAL_NAME = 1 << 1
        };

        epoc::buf_static<char16_t, 0x100> name_;

        std::uint32_t length_ = (static_cast<std::uint32_t>(des_type::buf) << 28) | sizeof(saddress);
        std::uint32_t max_length_ = sizeof(saddress);

        saddress addr_;

        std::uint32_t flags_ = 0;
    };
#pragma pack(pop)

    class host_resolver {
    public:
        virtual std::u16string host_name() const = 0;
        virtual bool host_name(const std::u16string &new_name) = 0;

        virtual bool get_by_address(saddress &addr, name_entry &result) = 0;
        virtual bool get_by_name(name_entry &supply_and_result) = 0;
        virtual bool next(name_entry &result) {
            return false;
        }
    };
}