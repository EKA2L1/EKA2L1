/*
 * Copyright (c) 2019 EKA2L1 Team
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

namespace eka2l1::crypt {
    /** 
     * \brief A simple CRC16 checksum.
     */
    void crc16(std::uint16_t &crc, const void *data, const std::size_t size);

    /**
     * \brief Encode binary data to ASCII with base64 algorithm.
     * 
     * 3 bytes are turned into 4 six-bits number, which is turned into ASCII.
     * 
     * Padding (=) will be added in case total bytes is not aligned to 3.
     * 
     * \param source        Binary data to be encoded.
     * \param source_size   Total bytes to be encoded.
     * \param dest          Pointer to where should contain the encoded data.
     *                      Specify nullptr for the function to estimate total dest bytes.
     * \param dest_max_size Maximum size of data, in bytes, that the dest can contain.
     *                      Ignored if dest pointer is null.
     * 
     * \returns Total bytes written to the dest.
     */
    std::size_t base64_encode(const std::uint8_t *source, const std::size_t source_size, char *dest,
        const std::size_t dest_max_size);

    /**
     * \brief Decode Base64 encoded data to its original form.
     * 
     * 4 bytes in ASCII, which is supposed to be four 6-bits number, will be turn back into
     * 3 eight bits number
     * 
     * "=" are a sign of padding and will be removed.
     * 
     * \param source        Binary data to be decoded.
     * \param source_size   Total bytes to be decoded. Must be aligned with 4.
     * \param dest          Pointer to where should contain the decoded data.
     *                      Specify nullptr for the function to estimate total dest bytes.
     * \param dest_max_size Maximum size of data, in bytes, that the dest can contain.
     *                      Ignored if dest pointer is null.
     * 
     * \returns Total bytes written to the dest. If source size is not aligned to 4, it will return 0.
     */
    std::size_t base64_decode(const std::uint8_t *source, const std::size_t source_size, char *dest,
        const std::size_t dest_max_size);
}
