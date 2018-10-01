/*
* Copyright (c) 2018 Vita3K Team / EKA2L1 Team.
*
* This file is part of Vita3K emulator project / EKA2L1 project
* (see bentokun.github.com/EKA2L1).
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
#include <array>

namespace eka2l1 {
	/*! \brief Contains function and class that help connecting between the host and guest (Symbian).
	*/
    namespace hle {
		/*! \brief The location of the argument in EABI */
        enum class arg_where {
            gpr,
            fpr,
            stack
        };

		/*! \brief The layout of the argument in EABI */
        struct arg_layout {
            arg_where loc;
            size_t offset;
        };

        template <typename... args>
        using args_layout = std::array<arg_layout, sizeof...(args)>;
    }
}
