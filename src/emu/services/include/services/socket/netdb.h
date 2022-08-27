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

#include <services/socket/common.h>
#include <utils/des.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    struct notify_info;
}

namespace eka2l1::epoc::socket {
    class net_database {
    public:
        virtual ~net_database() = default;

        virtual void query(const char *query_data, const std::uint32_t query_size, epoc::des8 *result_buffer, epoc::notify_info &complete_info) = 0;
        virtual void add(const char *record_buf, const std::uint32_t record_size, epoc::notify_info &complete_info) = 0;
        virtual void remove(const char *record_buf, const std::uint32_t record_size, epoc::notify_info &complete_info) = 0;
        virtual void cancel() = 0;
    };
}