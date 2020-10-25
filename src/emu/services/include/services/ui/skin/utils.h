/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <services/ui/skin/common.h>

#include <optional>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc {
    /**
     * \brief   Search all drives and find the skin file with specified package ID.
     * 
     * \param   io            Pointer to IO system.
     * \param   skin_pid      The PID of the skin we want to search the path.
     * 
     * \returns The path to the skin file with specified PID, else nullopt.
     */
    std::optional<std::u16string> find_skin_file(eka2l1::io_system *io, const epoc::pid skin_pid);

    /**
     * \brief Get the path to the resource folder of skin with specified package ID.
     * 
     * \param   io            Pointer to IO system.
     * \param   skin_pid      The PID of the skin we want to search the path.
     * 
     * \returns The path to the resource folder correspond to specified PID, else nullopt.
     */
    std::optional<std::u16string> get_resource_path_of_skin(eka2l1::io_system *io, const epoc::pid skin_pid);
}