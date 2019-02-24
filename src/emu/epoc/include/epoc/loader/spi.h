/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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
#include <vector>

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::loader {
    enum {
        SPI_UID = 0x10205C2B
    };

    /*! \brief Header of a resource archive files
    */
    struct spi_header {
        // First UID is the SPI UID.
        // The second contains SPI type
        std::uint32_t uids[3];
        std::uint32_t crc;

        explicit spi_header(const std::uint32_t type);
        bool do_state(common::chunkyseri &seri);
    };

    struct spi_entry {
        std::string name;
        std::vector<std::uint8_t> file;

        bool do_state(common::chunkyseri &seri);
    };

    /*! \breif An SPI object that can be seralized
     *
     * SPI are simply files container but for resource files.
     * It was mostly used to pack a big pack of resource files.
     * 
     * ECom use this.
    */
    struct spi_file {
        spi_header header;
        std::vector<spi_entry> entries;

    public:
        explicit spi_file(const std::uint32_t type)
            : header(type) {
        }

        bool do_state(common::chunkyseri &seri);
    };
}