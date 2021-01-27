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

namespace eka2l1::epoc::socket {
    struct socket {
    public:
        /**
         * @brief Get an option available from current socket.
         * 
         * @param option_id         The ID of the option.
         * @param option_family     The ID of the family that this option belongs to.
         * @param buffer            Destination to write data to.
         * @param avail_size        The total size that the destination buffer can hold.
         * 
         * @returns size_t(-1) if there is an error or not enough sufficient size to hold, else returns the total size written.
         */
        virtual std::size_t get_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) = 0;

        /**
         * @brief Set an option available from current socket.
         * 
         * @param option_id         The ID of the option.
         * @param option_family     The ID of the family that this option belongs to.
         * @param buffer            Data source to set.
         * @param avail_size        The total size that the source buffer holds.
         * 
         * @returns false on failure.
         */
        virtual bool set_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) = 0;
    };
}