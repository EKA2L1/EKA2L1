// Vita3K emulator project / EKA2l1 project
// Copyright (C) 2018 Vita3K team / 2018 EKA2l1 team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <array>

namespace eka2l1 {
    namespace hle {
        enum class arg_where {
            gpr,
            fpr,
            stack
        };

        struct arg_layout {
            arg_where loc;
            size_t offset;
        };

        template <typename... args>
        using args_layout = std::array<arg_layout, sizeof...(args)>;
    }
}