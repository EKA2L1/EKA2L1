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
     * @brief Find all skin files in indicated drives.
     *
     * @param io                Pointer to IO system.
     * @param drive_bitmask     The bitmask of the drives to search.
     *
     * @return The list of path to skin files found.
     */
    std::vector<std::u16string> find_skin_files(eka2l1::io_system *io, const std::uint32_t drive_bitmask);

    /**
     * @brief   Search the skin folders, and pick the first skin that's available.
     * 
     * @param   io              Pointer to IO system. Must not be null.
     * 
     * @returns The PID of the skin picked if there's one available.
     */
    std::optional<epoc::pid> pick_first_skin(eka2l1::io_system *io);

    /**
     * \brief Get the path to the resource folder of skin with specified package ID.
     * 
     * \param   io            Pointer to IO system.
     * \param   skin_pid      The PID of the skin we want to search the path.
     * 
     * \returns The path to the resource folder correspond to specified PID, else nullopt.
     */
    std::optional<std::u16string> get_resource_path_of_skin(eka2l1::io_system *io, const epoc::pid skin_pid);

    /**
     * @brief Produced a string containing the UID and timestamp (if available) in hex format.
     * 
     * Both convereted components are connected without a space.
     * 
     * @param       skin_pid      The PID to convert.
     * @returns     A string containg the converted PID.
     */
    std::u16string pid_to_string(const epoc::pid skin_pid);
}