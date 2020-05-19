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
#include <epoc/vfs.h>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::kernel {
    enum btrace_header_structure {
        btrace_header_size_index = 0,
        btrace_header_flag_index = 1,
        btrace_header_category_index = 2,
        btrace_header_subcategory_index = 3
    };

    struct btrace {
        io_system *io_;
        kernel_system *kern_;

        symfile trace_;

    public:
        explicit btrace(kernel_system *kern, io_system *io);
        ~btrace();

        bool start_trace_session(const std::u16string &trace_path);
        bool close_trace_session();

        bool out(const std::uint32_t a0, const std::uint32_t a1, const std::uint32_t a2,
            const std::uint32_t a3);
    };
}