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

#include <cstddef>
#include <cstdint>

namespace eka2l1::epoc::socket {
    /// Note! This is following Symbian OS es_sock ordinal
    enum socket_type {
        socket_type_undefined = 0,
        socket_type_stream = 1,     ///< Data sent in order, no boundaries, re-connect when socket sudden end (no goodbye)
        socket_type_datagram,       ///< Data sent may not be in order, message boundaries preserved, data may modified or lost
        socket_type_packet,         ///< Stream except message/data has length/boundaries.
        socket_type_raw             ///< Receive raw packet that is wrapped with protocol header, has not been extracted by any network layer.
    };

    enum {
        SOCKET_MESSAGE_SIZE_IS_STREAM = 0,          ///< Stream socket, message can be of any size
        SOCKET_MESSAGE_SIZE_UNDEFINED = 1,          ///< Undefined, depends on layers
        SOCKET_MESSAGE_SIZE_NO_LIMIT = -1
    };

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
            std::uint8_t *buffer, const std::size_t avail_size);

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
            std::uint8_t *buffer, const std::size_t avail_size);
    };
}