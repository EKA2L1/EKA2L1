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

#include <map>
#include <vector>

namespace eka2l1::common {
    class ro_stream;
    class wo_stream;
}

namespace eka2l1::epoc::msv {
    using store_buffer = std::vector<std::uint8_t>;

    class store {
    private:
        std::map<std::uint32_t, store_buffer> stores_;

    public:
        explicit store();

        bool read(common::ro_stream &stream);
        bool write(common::wo_stream &stream);

        store_buffer &buffer_for(const std::uint32_t uid);
        bool buffer_exists(const std::uint32_t uid);

        const std::size_t buffer_count() const {
            return stores_.size();
        }
    };
}