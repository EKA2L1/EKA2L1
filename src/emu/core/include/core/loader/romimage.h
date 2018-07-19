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
    struct file;
    class memory_system;
    class disasm;

    using symfile = std::shared_ptr<file>;

    namespace hle {
        class lib_manager;
    }

    namespace loader {
        // Unstable

        struct rom_vsec_info {
            uint32_t secure_id;
            uint32_t vendor_id;
            uint32_t cap1;
            uint32_t cap2;
        };

        struct rom_image_header {
            uint32_t uid1;
            uint32_t uid2;
            uint32_t uid3;
            uint32_t uid_checksum;
            uint32_t entry_point;
            uint32_t code_address;
            uint32_t data_address;
            int32_t code_size;
            int32_t text_size;
            int32_t data_size;
            int32_t bss_size;
            int32_t heap_minimum_size;
            int32_t heap_maximum_size;
            int32_t stack_size;
            uint32_t dll_ref_table_address;
            int32_t export_dir_count;
            uint32_t export_dir_address;
            uint32_t code_checksum;
            uint32_t data_checksum;
            rom_vsec_info sec_info;
            uint8_t major;
            uint8_t minor;
            uint16_t build;
            uint32_t flags_raw;
            uint32_t priority;
            uint32_t data_bss_linear_base_address;
            uint32_t next_extension_linear_address;
            uint32_t harware_variant; //I have no idea what this is exactly
            uint32_t total_size;
            uint32_t module;
            uint32_t exception_des;
        };

        struct romimg {
            rom_image_header header;
            std::vector<uint32_t> exports;
        };

        std::optional<romimg> parse_romimg(symfile &file, memory_system *mem);
    }
}

