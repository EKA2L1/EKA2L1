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
#include <memory>

#include <drivers/graphics/common.h>

namespace eka2l1::drivers {
    enum cursor_type {
        cursor_type_arrow,
        cursor_type_ibeam,
        cursor_type_vresize,
        cursor_type_hresize,
        cursor_type_hand,
        cursor_type_allresize,
        cursor_type_nesw_resize,
        cursor_type_nwse_resize
    };
    
    class cursor {
    public:
        virtual ~cursor() {};
        virtual void *raw_handle() = 0;
    };

    class cursor_controller {
    public:
        virtual std::unique_ptr<cursor> create(const cursor_type type) = 0;
    };

    std::unique_ptr<cursor_controller> make_new_cursor_controller(const window_api api);
}