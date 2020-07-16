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

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::epoc::aif {
    enum data_type_priority {
        data_type_priority_last_resort = -20000,
        data_type_priority_low = -10000,
        data_type_priority_normal = 0,
        data_type_priority_high = 10000
    };
    
    struct data_type {
        std::int32_t priority_;
        std::string type_;
    };

    struct view_data {
        std::uint32_t uid_;
        std::int32_t screen_mode_;
        std::uint32_t icon_count_;
        std::vector<std::u16string> caption_list_;
    };

    using file_ownership_list = std::vector<std::u16string>;
}
