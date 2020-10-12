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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace eka2l1 {
    class memory_system;
}

namespace eka2l1::loader {
    struct rom_vsec_info {
        uint32_t secure_id;
        uint32_t vendor_id;
        uint32_t cap1;
        uint32_t cap2;
    };

    struct rom_image_header {
        std::uint32_t uid1;
        std::uint32_t uid2;
        std::uint32_t uid3;
        std::uint32_t uid_checksum;
        std::uint32_t entry_point;
        std::uint32_t code_address;
        std::uint32_t data_address;
        std::int32_t code_size;
        std::int32_t text_size;
        std::int32_t data_size;
        std::int32_t bss_size;
        std::int32_t heap_minimum_size;
        std::int32_t heap_maximum_size;
        std::int32_t stack_size;
        std::uint32_t dll_ref_table_address;
        std::int32_t export_dir_count;
        std::uint32_t export_dir_address;
        rom_vsec_info sec_info;
        std::uint32_t checksum_code;
        std::uint32_t checksum_data;
        std::uint8_t major;
        std::uint8_t minor;
        std::uint16_t build;
        std::uint32_t flags_raw;
        std::uint32_t priority;
        std::uint32_t data_bss_linear_base_address;
        std::uint32_t next_extension_linear_address;
        std::uint32_t hardware_variant; //I have no idea what this is exactly
        std::uint32_t total_size;
        std::uint32_t module;
        std::uint32_t exception_des;
    };

    struct romimg {
        rom_image_header header;
        std::vector<uint32_t> exports;
    };

    std::optional<romimg> parse_romimg(common::ro_stream *stream, memory_system *mem, const epocver os_ver);
}
