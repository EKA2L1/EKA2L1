/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <string>
#include <string_view>

namespace eka2l1::control {
    /**
     * \brief Write data to a host file that does not exist yet.
     *
     * Nothing already at the path is replaced, truncated or followed, a symbolic link
     * included. Throws rpc_error -32003 (failed), naming the path, when something is there or
     * the file cannot be written. A file this call created but could not write whole is left
     * in place, and the message says so.
     */
    void write_new_file(const std::string &path, std::string_view data);
}
