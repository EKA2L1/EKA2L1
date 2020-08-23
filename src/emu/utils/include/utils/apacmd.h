/*
 * Copyright (c) 2020 EKA2L1 Team.
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

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::epoc::apa {
    enum command {
        command_open = 0,
        command_create = 1,
        command_run = 2,
        command_background = 3,
        command_view_activate = 4,
        command_run_without_views = 5,
        command_background_without_views = 6
    };

    struct command_line {
        command launch_cmd_;
        std::uint32_t server_differentiator_;
        std::int32_t default_screen_number_;
        std::uint32_t parent_window_group_id_;
        std::int32_t debug_mem_fail_;
        std::uint32_t app_startup_instrumentation_event_id_base_;
        std::int32_t parent_process_id_;

        std::u16string executable_path_;     ///< Path of app (like Python file, MIDLET, C++ host app, etc...)
        std::u16string document_name_;
        std::string opaque_data_;
        std::string tail_end_;
        
        explicit command_line();

        void do_it(common::chunkyseri &seri);
    }
}