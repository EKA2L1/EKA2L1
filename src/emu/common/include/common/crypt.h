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
#include <string>

namespace eka2l1::crypt {
    enum imei_valid_error {
        IMEI_ERROR_NONE = 0, ///< No error occured.
        IMEI_ERROR_NO_RIGHT_LENGTH = -1, ///< Length of sequence is not 15.
        IMEI_ERROR_INVALID_SUM = -2, ///< Total sum doubled module 10 is not 0.
        IMEI_ERROR_INVALID_CHARACTER = -3 ///< At least one non-numeric character is in the sequence.
    };

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

    /**
     * @brief Encode binary data to ASCII with base64 algorithm.
     * 
     * This function will store the result to string. For more advanced use that need to output to another kind of buffer directly,
     * you can use the other counterpart.
     * 
     * @param source The source binary data to be encoded.
     * @param source_size The size of the source buffer.
     * 
     * @return std::string Encoded base64 string. 
     */
    std::string base64_encode(const std::uint8_t *source, const std::size_t source_size);

    /**
     * @brief   Calculate checksum of 3 numbers UID.
     * 
     * @param   uids      Pointer to the UID buffer.
     * @return  Checksum of the UID buffer.
     */
    std::uint32_t calculate_checked_uid_checksum(const std::uint32_t *uids);

    /**
     * @brief Check if given IMEI string is correct
     * 
     * @returns IMEI_ERROR_NONE on right.
     */
    imei_valid_error is_imei_valid(const std::string &supposed_imei);
}
