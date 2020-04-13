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

#include <common/crypt.h>
#include <string>

namespace eka2l1::crypt {
    static const std::uint32_t crc_tab[256] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a,
        0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294,
        0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462,
        0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509,
        0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695,
        0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5,
        0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948,
        0x9969, 0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4,
        0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b,
        0x8d68, 0x9d49, 0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 0xff9f,
        0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb,
        0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046,
        0x6067, 0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x02b1, 0x1290,
        0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e,
        0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x0691,
        0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
        0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d,
        0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16,
        0x0af1, 0x1ad0, 0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8,
        0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1, 0xef1f, 0xff3e,
        0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93,
        0x3eb2, 0x0ed1, 0x1ef0
    };

    void crc16(std::uint16_t &crc, const void *data, const std::size_t size) {
        const std::uint8_t *cur = reinterpret_cast<const std::uint8_t *>(data);
        const std::uint8_t *end = reinterpret_cast<const std::uint8_t *>(data) + size;

        while (cur < end) {
            crc = (crc << 8) ^ (crc_tab[((crc >> 8) ^ *cur++) & 0xff]);
        }
    }

    static const std::string base64_ascii_map = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::size_t base64_encode(const std::uint8_t *source, const std::size_t source_size, char *dest,
        const std::size_t dest_max_size) {
        std::size_t dest_written = 0;

        for (std::size_t i = 0; i < source_size; i += 3) {
            const std::uint8_t b1 = source[i];
            const std::uint8_t b2 = (i + 1 > source_size) ? 0 : source[i + 1];
            const std::uint8_t b3 = (i + 2 > source_size) ? 0 : source[i + 2];

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = base64_ascii_map[(b1 & 0b11111100) >> 2];
            }

            dest_written++;

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = base64_ascii_map[((b1 & 0b11) << 4) | (b2 & 0b11110000) >> 4];
            }

            dest_written++;

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = base64_ascii_map[((b2 & 0b1111) << 2) | (b3 & 0b11000000) >> 6];
            }

            dest_written++;

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = base64_ascii_map[(b3 & 0b111111)];
            }

            dest_written++;
        }

        if (dest && source_size % 3 != 0) {
            for (std::size_t i = 0; i < 3 - source_size % 3; i++) {
                dest[dest_written - i - 1] = '=';
            }
        }

        return dest_written;
    }

    std::size_t base64_decode(const std::uint8_t *source, const std::size_t source_size, char *dest,
        const std::size_t dest_max_size) {
        std::size_t dest_written = 0;

        if (source_size % 4 != 0) {
            return dest_written;
        }

        for (std::size_t i = 0; i < source_size; i += 4) {
            const std::uint8_t b0 = static_cast<std::uint8_t>(base64_ascii_map.find(source[i]));
            const std::uint8_t b1 = static_cast<std::uint8_t>(base64_ascii_map.find(source[i + 1]));
            const std::uint8_t b2 = source[i + 2] == '=' ? 0 : static_cast<std::uint8_t>(base64_ascii_map.find(source[i + 2]));
            const std::uint8_t b3 = source[i + 3] == '=' ? 0 : static_cast<std::uint8_t>(base64_ascii_map.find(source[i + 3]));

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4309)
#endif
            if (b0 == static_cast<std::uint8_t>(std::string::npos) || b1 == static_cast<std::uint8_t>(std::string::npos) || b2 == static_cast<std::uint8_t>(std::string::npos) || b3 == static_cast<std::uint8_t>(std::string::npos)) {
                return dest_written;
            }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = ((b0 & 0b111111) << 2) | ((b1 & 0b00110000) >> 4);
            }

            dest_written++;

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = ((b1 & 0b1111) << 4) | ((b2 & 0b00111100) >> 2);
            }

            dest_written++;

            if (dest) {
                if (dest_written == dest_max_size) {
                    return dest_written;
                }

                dest[dest_written] = ((b2 & 0b11) << 6) | (b3 & 0b00111111);
            }

            dest_written++;
        }

        for (std::size_t i = 0; i < 2; i++) {
            if (source[source_size - i - 1] == '=') {
                dest_written--;
            }
        }

        return dest_written;
    }
}
