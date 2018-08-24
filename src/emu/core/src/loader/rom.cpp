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
#include <common/log.h>

#include <core/loader/rom.h>

namespace eka2l1 {
    namespace loader {
        enum class file_attrib {
            dir = 0x0010
        };

        uint32_t rom_to_offset(address romstart, address off) {
            return off - romstart;
        }

        rom_header read_rom_header(FILE *file) {
            rom_header header;

            fread(header.jump, 1, sizeof(header.jump), file);
            fread(&header.restart_vector, 1, 4, file);
            fread(&header.time, 1, 8, file);
            fread(&header.time_high, 1, 4, file);
            fread(&header.rom_base, 1, 4, file);
            fread(&header.rom_size, 1, 4, file);
            fread(&header.rom_root_dir_list, 1, 4, file);
            fread(&header.kern_data_address, 1, 4, file);
            fread(&header.kern_limit, 1, 4, file);
            fread(&header.primary_file, 1, 4, file);
            fread(&header.secondary_file, 1, 4, file);
            fread(&header.checksum, 1, 4, file);

            // From this section, all read are invalid

            // Symbian says those are for testing though
            fread(&header.hardware, 1, 4, file);
            fread(&header.lang, 1, 8, file);

            fread(&header.kern_config_flags, 1, 4, file);
            fread(&header.rom_exception_search_tab, 1, 4, file);
            fread(&header.rom_header_size, 1, 4, file);

            fread(&header.rom_section_header, 1, 4, file);
            fread(&header.total_sv_data_size, 1, 4, file);
            fread(&header.variant_file, 1, 4, file);
            fread(&header.extension_file, 1, 4, file);
            fread(&header.reloc_info, 1, 4, file);

            fread(&header.old_trace_mask, 1, 4, file);
            fread(&header.user_data_addr, 1, 4, file);
            fread(&header.total_user_data_size, 1, 4, file);

            fread(&header.debug_port, 1, 4, file);
            fread(&header.major, 1, 1, file);
            fread(&header.minor, 1, 1, file);
            fread(&header.build, 1, 2, file);

            fread(&header.compress_type, 1, 4, file);
            fread(&header.compress_size, 1, 4, file);
            fread(&header.uncompress_size, 1, 4, file);
            fread(&header.disabled_caps, 4, 2, file);
            fread(&header.trace_mask, 4, 8, file);
            fread(&header.initial_btrace_filter, 1, 4, file);

            fread(&header.initial_btrace_buf, 1, 4, file);
            fread(&header.initial_btrace_mode, 1, 4, file);

            fread(&header.pageable_rom_start, 1, 4, file);
            fread(&header.pageable_rom_size, 1, 4, file);

            fread(&header.rom_page_idx, 1, 4, file);
            fread(&header.compressed_unpaged_start, 1, 4, file);

            fread(&header.unpaged_compressed_size, 1, 4, file);
            fread(&header.hcr_file_addr, 1, 4, file);
            fread(header.spare, 4, 36, file);

            return header;
        }

        rom_dir read_rom_dir(rom &romf);

        rom_entry read_rom_entry(rom &romf, rom_dir *mother) {
            rom_entry entry;
            FILE *file = romf.handler;

            fread(&entry.size, 1, 4, file);
            fread(&entry.address_lin, 1, 4, file);
            fread(&entry.attrib, 1, 1, file);
            fread(&entry.name_len, 1, 1, file);

            entry.name.resize(entry.name_len);

            fread(entry.name.data(), 2, entry.name_len, file);

            if (entry.attrib & (int)file_attrib::dir) {
                auto crr_pos = ftell(file);
                fseek(file, rom_to_offset(romf.header.rom_base, entry.address_lin), SEEK_SET);
                entry.dir = std::make_optional<rom_dir>(read_rom_dir(romf));
                entry.dir->name = entry.name;
                mother->subdirs.push_back(entry.dir.value());
                fseek(file, crr_pos, SEEK_SET);
            }

            return entry;
        }

        rom_dir read_rom_dir(rom &romf) {
            rom_dir dir;

            auto old_off = ftell(romf.handler);

            fread(&dir.size, 1, 4, romf.handler);

            while (ftell(romf.handler) - old_off < dir.size) {
                dir.entries.push_back(read_rom_entry(romf, &dir));

                if (ftell(romf.handler) % 4 != 0) {
                    fseek(romf.handler, 2, SEEK_CUR);
                }
            }

            // Sort this for lower_bound binary search
            std::sort(dir.entries.begin(), dir.entries.end(), [](const rom_entry &lhs, const rom_entry &rhs) {
                return common::compare_ignore_case(lhs.name, rhs.name) == -1;
            });

            // Sort this for lower_bound binary search
            std::sort(dir.subdirs.begin(), dir.subdirs.end(), [](const rom_dir &lhs, const rom_dir &rhs) {
                return common::compare_ignore_case(lhs.name, rhs.name) == -1;
            });

            return dir;
        }

        root_dir read_root_dir(rom &romf) {
            root_dir rdir;
            FILE *file = romf.handler;

            fread(&rdir.hardware_variant, 1, 4, file);
            fread(&rdir.addr_lin, 1, 4, file);

            fseek(file, rom_to_offset(romf.header.rom_base, rdir.addr_lin), SEEK_SET);

            rdir.dir = read_rom_dir(romf);

            return rdir;
        }

        root_dir_list read_root_dir_list(rom &romf) {
            root_dir_list list;
            FILE *file = romf.handler;

            fread(&list.num_root_dirs, 1, 4, file);

            for (int i = 0; i < list.num_root_dirs; i++) {
                auto last_pos = ftell(file);
                list.root_dirs.push_back(read_root_dir(romf));
                fseek(file, last_pos, SEEK_SET);
            }

            // Sort this for lower_bound binary search
            std::sort(list.root_dirs.begin(), list.root_dirs.end(), [](const root_dir &lhs, const root_dir &rhs) {
                return (common::compare_ignore_case(lhs.dir.name, rhs.dir.name) == -1);
            });

            return list;
        }

        // Loading rom supports only uncompressed rn
        std::optional<rom> load_rom(const std::string &path) {
            rom romf;
            romf.handler = fopen(path.c_str(), "rb");

            if (!romf.handler) {
                return std::optional<rom>{};
            }

            romf.header = read_rom_header(romf.handler);

            // Seek to the first entry
            fseek(romf.handler, rom_to_offset(romf.header.rom_base, romf.header.rom_root_dir_list), SEEK_SET);

            romf.root = read_root_dir_list(romf);

            return romf;
        }
    }
}
