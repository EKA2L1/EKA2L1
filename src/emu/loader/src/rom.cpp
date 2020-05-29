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

#include <loader/rom.h>

namespace eka2l1::loader {
    enum class file_attrib {
        dir = 0x0010
    };

    uint32_t rom_to_offset(address romstart, address off) {
        return off - romstart;
    }

    static rom_header read_rom_header(common::ro_stream *stream) {
        rom_header header;

        [[maybe_unused]] std::size_t readed_size = 0;

        readed_size += stream->read(header.jump, sizeof(header.jump));
        readed_size += stream->read(&header.restart_vector, 4);
        readed_size += stream->read(&header.time, 8);
        readed_size += stream->read(&header.time_high, 4);
        readed_size += stream->read(&header.rom_base, 4);
        readed_size += stream->read(&header.rom_size, 4);
        readed_size += stream->read(&header.rom_root_dir_list, 4);
        readed_size += stream->read(&header.kern_data_address, 4);
        readed_size += stream->read(&header.kern_limit, 4);
        readed_size += stream->read(&header.primary_file, 4);
        readed_size += stream->read(&header.secondary_file, 4);
        readed_size += stream->read(&header.checksum, 4);

        // From this section, all read are weird
        // TODO: Check the spec again
        readed_size += stream->read(&header.hardware, 4);
        readed_size += stream->read(&header.lang, 8);

        readed_size += stream->read(&header.kern_config_flags, 4);
        readed_size += stream->read(&header.rom_exception_search_tab, 4);
        readed_size += stream->read(&header.rom_header_size, 4);

        readed_size += stream->read(&header.rom_section_header, 4);
        readed_size += stream->read(&header.total_sv_data_size, 4);
        readed_size += stream->read(&header.variant_file, 4);
        readed_size += stream->read(&header.extension_file, 4);
        readed_size += stream->read(&header.reloc_info, 4);

        readed_size += stream->read(&header.old_trace_mask, 4);
        readed_size += stream->read(&header.user_data_addr, 4);
        readed_size += stream->read(&header.total_user_data_size, 4);

        readed_size += stream->read(&header.debug_port, 4);
        readed_size += stream->read(&header.major, 1);
        readed_size += stream->read(&header.minor, 1);
        readed_size += stream->read(&header.build, 2);

        readed_size += stream->read(&header.compress_type, 4);
        readed_size += stream->read(&header.compress_size, 4);
        readed_size += stream->read(&header.uncompress_size, 4);
        readed_size += stream->read(&header.disabled_caps, 2);
        readed_size += stream->read(&header.trace_mask, 8);
        readed_size += stream->read(&header.initial_btrace_filter, 4);

        readed_size += stream->read(&header.initial_btrace_buf, 4);
        readed_size += stream->read(&header.initial_btrace_mode, 4);

        readed_size += stream->read(&header.pageable_rom_start, 4);
        readed_size += stream->read(&header.pageable_rom_size, 4);

        readed_size += stream->read(&header.rom_page_idx, 4);
        readed_size += stream->read(&header.compressed_unpaged_start, 4);

        readed_size += stream->read(&header.unpaged_compressed_size, 4);
        readed_size += stream->read(&header.hcr_file_addr, 4);
        readed_size += stream->read(header.spare, 4 * 36);

        // TODO: Gonna do something with readed size
        // I suppose i can void all these stream->read, but I don't like to do so

        return header;
    }

    static rom_dir read_rom_dir(rom &romf, common::ro_stream *stream);

    static rom_entry read_rom_entry(rom &romf, rom_dir *mother, common::ro_stream *stream) {
        rom_entry entry;

        std::size_t readed_size = 0;

        readed_size += stream->read(&entry.size, 4);
        readed_size += stream->read(&entry.address_lin, 4);
        readed_size += stream->read(&entry.attrib, 1);
        readed_size += stream->read(&entry.name_len, 1);

        if (readed_size != 10) {
            LOG_ERROR("Can't read entry header!");
            return entry;
        }

        readed_size = 0;

        entry.name.resize(entry.name_len);

        if (stream->read(entry.name.data(), 2 * entry.name_len) != (2 * entry.name_len)) {
            LOG_ERROR("Can't read entry name!");
        }

        if (entry.attrib & static_cast<int>(file_attrib::dir)) {
            const auto crr_pos = stream->tell();
            stream->seek(rom_to_offset(romf.header.rom_base, entry.address_lin), common::seek_where::beg);

            entry.dir = std::make_optional<rom_dir>(read_rom_dir(romf, stream));
            entry.dir->name = entry.name;
            mother->subdirs.push_back(entry.dir.value());

            stream->seek(crr_pos, common::seek_where::beg);
        }

        return entry;
    }

    static rom_dir read_rom_dir(rom &romf, common::ro_stream *stream) {
        rom_dir dir;

        const auto old_off = stream->tell();

        if (stream->read(&dir.size, 4) != 4) {
            LOG_ERROR("Can't read directory size!");
            return dir;
        }

        while (stream->tell() - old_off < dir.size) {
            dir.entries.push_back(read_rom_entry(romf, &dir, stream));

            if (stream->tell() % 4 != 0) {
                stream->seek(2, common::seek_where::cur);
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

    static root_dir read_root_dir(rom &romf, common::ro_stream *stream) {
        root_dir rdir;

        if (stream->read(&rdir.hardware_variant, 4) != 4) {
            LOG_ERROR("Can't read hardware variant of root directory!");
            return rdir;
        }

        if (stream->read(&rdir.addr_lin, 4) != 4) {
            LOG_ERROR("Can't read linear address of root directory!");
            return rdir;
        }

        stream->seek(rom_to_offset(romf.header.rom_base, rdir.addr_lin), common::seek_where::beg);
        rdir.dir = read_rom_dir(romf, stream);

        return rdir;
    }

    static root_dir_list read_root_dir_list(rom &romf, common::ro_stream *stream) {
        root_dir_list list;

        if (stream->read(&list.num_root_dirs, 4) != 4) {
            LOG_ERROR("Can't read number of directories in root directory!");
            return list;
        }

        for (int i = 0; i < list.num_root_dirs; i++) {
            const auto last_pos = stream->tell();
            list.root_dirs.push_back(read_root_dir(romf, stream));
            stream->seek(last_pos, common::seek_where::beg);
        }

        // Sort this for lower_bound binary search
        std::sort(list.root_dirs.begin(), list.root_dirs.end(), [](const root_dir &lhs, const root_dir &rhs) {
            return (common::compare_ignore_case(lhs.dir.name, rhs.dir.name) == -1);
        });

        return list;
    }

    std::optional<rom> load_rom(common::ro_stream *stream) {
        rom romf;
        romf.header = read_rom_header(stream);

        // Seek to the first entry
        stream->seek(rom_to_offset(romf.header.rom_base, romf.header.rom_root_dir_list),
            common::seek_where::beg);

        romf.root = read_root_dir_list(romf, stream);

        return romf;
    }
}
