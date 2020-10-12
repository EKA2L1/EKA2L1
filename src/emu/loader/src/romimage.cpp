/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/log.h>

#include <loader/romimage.h>
#include <mem/ptr.h>

namespace eka2l1::loader {
    std::optional<romimg> parse_romimg(common::ro_stream *stream, memory_system *mem, const epocver os_ver) {
        romimg img;

        img.header.checksum_code = 0;
        img.header.checksum_data = 0;
        img.header.sec_info.cap1 = 0;
        img.header.sec_info.cap2 = 0;
        img.header.sec_info.secure_id = 0;
        img.header.sec_info.vendor_id = 0;

        std::uint32_t size_to_initially_read = offsetof(rom_image_header, sec_info);
        if (stream->read(&(img.header), size_to_initially_read) != size_to_initially_read) {
            return std::nullopt;
        }

        // Read platform difference
        if (os_ver <= epocver::eka2) {
            if (stream->read(&img.header.checksum_code, 4) != 4) {
                return std::nullopt;
            }

            if (stream->read(&img.header.checksum_data, 4) != 4) {
                return std::nullopt;
            }
        } else {
            if (stream->read(&img.header.sec_info, sizeof(rom_vsec_info)) != sizeof(rom_vsec_info)) {
                return std::nullopt;
            }
        }

        // Read the rest
        size_to_initially_read = offsetof(rom_image_header, exception_des) - offsetof(rom_image_header, major) + sizeof(rom_image_header::exception_des);
        if (stream->read(&img.header.major, size_to_initially_read) != size_to_initially_read) {
            return std::nullopt;
        }

        ptr<uint32_t> export_off(img.header.export_dir_address);

        for (int32_t i = 0; i < img.header.export_dir_count; i++) {
            auto export_addr = *export_off.get(mem);
            img.exports.push_back(export_addr);

            export_off += 4;
        }

        return img;
    }
}
