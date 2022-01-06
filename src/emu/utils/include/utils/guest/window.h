/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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
#include <mem/ptr.h>

namespace eka2l1::utils {
    struct window_buffer_base;
    struct window_session;

    struct window_client_interface {
        std::uint32_t handle_;
        eka2l1::ptr<window_buffer_base> buffer_;     

        window_session *get_window_session(kernel::process *pr);  
    };

    struct window_session : public window_client_interface {
        std::uint32_t session_handle_;
    };

    struct window_buffer_base {
        eka2l1::ptr<window_session> session_;
        // The rest is nothing to care... For now (tadum)
    };
}