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

#include <common/types.h>
#include <loader/common.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eka2l1 {
    class memory_system;
    class kernel_system;

    namespace common {
        class ro_stream;
    }

    /*! \brief Contains the loader for E32Image, ROMImage, SIS. */
    namespace loader {
        enum class e32_cpu : uint16_t {
            x86 = 0x1000,
            armv4 = 0x2000,
            armv5 = 0x2001,
            armv6 = 0x2002,
            mstar_core = 0x4000
        };

        enum class e32_flags : uint32_t {
            exe = 0,
            dll = 1
        };

        enum class e32_img_type : uint32_t {
            exe = 0x1000007a,
            dll = 0x10000079
        };

        // Copy from E32Explorer
        struct e32img_import_block {
            //This is only for EKA1 targeted import block from PE
            uint32_t dll_name_offset; //relative to the import section
            int32_t number_of_imports; // number of imports from this dll
            std::vector<uint32_t> ordinals; // TUint32 iImport[iNumberOfImports];
            std::string dll_name;
        };

        struct e32img_vsec_info {
            uint32_t secure_id;
            uint32_t vendor_id;
            uint32_t cap1;
            uint32_t cap2;
        };

        struct e32img_header_extended {
            e32img_vsec_info info;
            uint32_t exception_des;
            uint32_t spare2;
            uint16_t export_desc_size; // size of bitmap section
            uint8_t export_desc_type; // type of description of holes in export table
            uint8_t export_desc;
        };

        struct e32_import_section {
            int32_t size; // size of this section
            std::vector<e32img_import_block> imports; // E32ImportBlock[iDllRefTableCount];
        };

        // For my precious baby to yml
        struct e32img_export_directory {
            std::vector<uint32_t> syms;
        };

        struct e32img_iat {
            uint32_t number_imports;
            std::vector<uint32_t> its;
        };

        struct e32_reloc_entry {
            uint32_t base;
            uint32_t size;

            std::vector<uint16_t> rels_info;
        };

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

        struct e32_reloc_section {
            uint32_t size;
            uint32_t num_relocs;

            std::vector<e32_reloc_entry> entries;
        };

        struct e32img_header {
            e32_img_type uid1;
            std::uint32_t uid2;
            std::uint32_t uid3;
            std::uint32_t check;
            std::uint32_t sig;

            //e32_cpu cpu;

            std::uint32_t header_crc;

            std::uint16_t major;
            std::uint16_t minor;

            std::uint32_t compression_type;

            std::uint8_t petran_major;
            std::uint8_t petran_minor;
            std::uint16_t petran_build;

            std::uint32_t msb;
            std::uint32_t lsb;

            std::uint32_t flags;
            std::uint32_t code_size;
            std::uint32_t data_size;

            std::uint32_t heap_size_min;
            std::uint32_t heap_size_max;

            std::uint32_t stack_size;
            std::uint32_t bss_size;
            std::uint32_t entry_point;

            std::uint32_t code_base;
            std::uint32_t data_base;
            std::uint32_t dll_ref_table_count;

            std::uint32_t export_dir_offset;
            std::uint32_t export_dir_count;

            std::uint32_t text_size;
            std::uint32_t code_offset;
            std::uint32_t data_offset;

            std::uint32_t import_offset;
            std::uint32_t code_reloc_offset;
            std::uint32_t data_reloc_offset;

            std::uint16_t priority;
            e32_cpu cpu;
        };

        struct e32img {
            epocver epoc_ver;

            e32img_header header;
            e32img_header_extended header_extended;
            e32img_iat iat;
            e32img_export_directory ed;

            std::vector<char> data;
            uint32_t uncompressed_size;
            e32_import_section import_section;

            e32_reloc_section code_reloc_section;
            e32_reloc_section data_reloc_section;

            uint32_t rt_code_addr;
            uint32_t rt_data_addr;

            uint32_t code_chunk;
            uint32_t data_chunk;

            bool has_extended_header = false;

            std::vector<std::string> dll_names;
        };

        /**
         * @brief Parse an E32 Image from stream.
         * 
         * @param stream     The stream to parse from.
         * @param read_reloc If this is true, relocation section will be parsed.
         * 
         * @returns An optional contains E32 Image. Nullopt if invalid.
         */
        std::optional<e32img> parse_e32img(common::ro_stream *stream, bool read_reloc = true);

        /**
         * @brief Check if the stream content is E32 Image.
         * 
         * @param   stream          Pointer to target read-only stream.
         * @param   uid_array       Optional pointer to the array, which will contains the first
         *                          three UIDs, even if the stream content is not E32IMG.
         *                          
         * 
         * @returns True if the stream content is E32IMG.
         */
        bool is_e32img(common::ro_stream *stream, std::uint32_t *uid_array = nullptr);
    }
}
