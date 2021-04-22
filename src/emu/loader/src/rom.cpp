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
#include <common/bytepair.h>
#include <common/log.h>
#include <common/path.h>
#include <common/cvt.h>

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
        readed_size += stream->read(&header.eka2_diff1, sizeof(header.eka2_diff1));

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
        readed_size += stream->read(&header.disabled_caps, sizeof(header.disabled_caps));
        readed_size += stream->read(&header.trace_mask, sizeof(header.trace_mask));
        readed_size += stream->read(&header.initial_btrace_filter, sizeof(header.initial_btrace_filter));

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
            LOG_ERROR(LOADER, "Can't read entry header!");
            return entry;
        }

        readed_size = 0;

        entry.name.resize(entry.name_len);

        if (stream->read(entry.name.data(), 2 * entry.name_len) != (2 * entry.name_len)) {
            LOG_ERROR(LOADER, "Can't read entry name!");
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
            LOG_ERROR(LOADER, "Can't read directory size!");
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
            LOG_ERROR(LOADER, "Can't read hardware variant of root directory!");
            return rdir;
        }

        if (stream->read(&rdir.addr_lin, 4) != 4) {
            LOG_ERROR(LOADER, "Can't read linear address of root directory!");
            return rdir;
        }

        stream->seek(rom_to_offset(romf.header.rom_base, rdir.addr_lin), common::seek_where::beg);
        rdir.dir = read_rom_dir(romf, stream);

        return rdir;
    }

    static root_dir_list read_root_dir_list(rom &romf, common::ro_stream *stream) {
        root_dir_list list;

        if (stream->read(&list.num_root_dirs, 4) != 4) {
            LOG_ERROR(LOADER, "Can't read number of directories in root directory!");
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

    int defrag_rom(common::ro_stream *stream, common::wo_stream *dest_stream) {
        std::optional<rom> rom_parse = load_rom(stream);
        if (!rom_parse.has_value()) {
            return ROM_DEFRAG_ERROR_INVALID_ROM;
        }

        static constexpr std::uint32_t EKA1_ROM_BASE = 0x50000000;
        if (rom_parse->header.rom_base == EKA1_ROM_BASE) {
            return 0;
        }

        if (rom_parse->header.compress_type != 0) {
            LOG_ERROR(LOADER, "ROM is wholely compressed with type 0x{:X}, currently unsupported", rom_parse->header.compress_type);
            return ROM_DEFRAG_ERROR_UNSUPPORTED;
        }

        if (!rom_parse->header.rom_page_idx) {
            return 0;
        }

        stream->seek(0, common::seek_where::beg);

        // The unpaged part of the ROM should be copied identically
        static constexpr std::int64_t CHUNK_SIZE = 0x10000;
        std::vector<std::uint8_t> buffer;
        std::int64_t left = rom_parse->header.pageable_rom_start;
        
        while (left > 0) {
            const std::int64_t take = common::min(left, CHUNK_SIZE);
            buffer.resize(take);

            if (stream->read(buffer.data(), take) != take) {
                return ROM_DEFRAG_ERROR_READ_WRITE_FAIL;
            }

            if (dest_stream->write(buffer.data(), take) != take) {
                return ROM_DEFRAG_ERROR_READ_WRITE_FAIL;
            }

            left -= take;
        }

        static constexpr std::uint32_t PAGE_SIZE = 0x1000;

        const std::uint32_t total_page_to_seek = (rom_parse->header.uncompress_size - rom_parse->header.pageable_rom_start
            + PAGE_SIZE - 1) / PAGE_SIZE;
        const std::uint32_t page_start = rom_parse->header.pageable_rom_start / PAGE_SIZE;

        buffer.resize(PAGE_SIZE);
        std::vector<std::uint8_t> source_buffer;

        for (std::size_t i = 0; i < total_page_to_seek; i++) {
            stream->seek(rom_parse->header.rom_page_idx + (page_start + i) * sizeof(rom_page_info),
                common::seek_where::beg);

            rom_page_info info;
            if (stream->read(&info, sizeof(info)) != sizeof(info)) {
                return 0;
            }

            if (!(info.paging_attrib & rom_page_info::attrib::pageable)) {
                LOG_ERROR(LOADER, "Page {} is not pageable!", page_start + i);
                return ROM_DEFRAG_ERROR_UNSUPPORTED;
            }

            if (info.data_size == 0) {
                std::fill(buffer.begin(), buffer.end(), 0xFF);
            } else {
                stream->seek(info.data_start, common::seek_where::beg);
                source_buffer.resize(info.data_size);

                if (stream->read(source_buffer.data(), info.data_size) != info.data_size) {
                    return ROM_DEFRAG_ERROR_READ_WRITE_FAIL;
                }

                if (info.compression_type == rom_page_info::compression::none) {
                    std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
                } else {
                    const int result = common::bytepair_decompress(buffer.data(), static_cast<std::uint32_t>(buffer.size()),
                        source_buffer.data(), static_cast<std::uint32_t>(source_buffer.size()));

                    if (result < 0) {
                        LOG_ERROR(LOADER, "Decompression failed for page {}!", page_start + i);
                        return ROM_DEFRAG_ERROR_READ_WRITE_FAIL;
                    }
                }
            }

            if (dest_stream->write(buffer.data(), buffer.size()) != buffer.size()) {
                return ROM_DEFRAG_ERROR_READ_WRITE_FAIL;
            }
        }

        return 1;
    }

    static bool dump_rom_file(common::ro_stream *stream, rom_entry &entry, const std::string &path_base, const std::uint32_t rom_base,
        std::atomic<int> &progress, const int base_progress, const int max_progress) {
        std::u16string fname = entry.name;
        if (common::is_platform_case_sensitive()) {
            fname = common::lowercase_ucs2_string(fname);
        }

        if (fname == u"Hal.dll") {
            LOG_TRACE(LOADER, "Just some interesting test");
        }

        fname = eka2l1::add_path(common::utf8_to_ucs2(path_base), fname);

        std::ofstream file_out_stream(common::ucs2_to_utf8(fname), std::ios_base::binary);
        stream->seek(entry.address_lin - rom_base, common::seek_where::beg);

        static constexpr std::int64_t MAX_CHUNK_SIZE = 0x10000;
        std::int64_t size_left = static_cast<std::int64_t>(entry.size);

        std::vector<char> buf;

        while (size_left > 0) {
            const std::int64_t size_take = common::min(MAX_CHUNK_SIZE, size_left);
            buf.resize(size_take);

            stream->read(buf.data(), buf.size());
            file_out_stream.write(buf.data(), buf.size());

            progress = common::max(progress.load(), base_progress + static_cast<int>(stream->tell() * max_progress / stream->size()));

            size_left -= size_take;
        }

        return true;
    }

    static bool dump_rom_directory(common::ro_stream *stream, rom_dir &dir, std::string base, std::uint32_t rom_base,
        std::atomic<int> &progress, const int base_progress, const int max_progress) {
        eka2l1::create_directories(base);

        for (auto &entry: dir.entries) {
            if (!(entry.attrib & 0x10) && (entry.size != 0)) {
                if (!dump_rom_file(stream, entry, base, rom_base, progress, base_progress, max_progress)) {
                    return false;
                }
            }
        }

        for (auto &subdir: dir.subdirs) {
            std::string new_base;

            if (common::is_platform_case_sensitive()) {
                new_base = eka2l1::add_path(base, common::lowercase_string(common::ucs2_to_utf8(
                    subdir.name)));
            } else {
                new_base = eka2l1::add_path(base, common::ucs2_to_utf8(subdir.name));
            }

            if (!dump_rom_directory(stream, subdir, new_base, rom_base, progress, base_progress, max_progress)) {
                return false;
            }
        }

        return true;
    }

    bool dump_rom_files(common::ro_stream *stream, const std::string &dest_base, std::atomic<int> &progress,
        const int max_progress) {
        std::optional<rom> rom_parse_result = load_rom(stream);
        if (!rom_parse_result.has_value()) {
            return false;
        }

        int base_progress = progress.load();

        for (root_dir &rdir: rom_parse_result->root.root_dirs) {
            if (!dump_rom_directory(stream, rdir.dir, dest_base, rom_parse_result->header.rom_base, progress, base_progress, max_progress)) {
                return false;
            }
        }

        // Make it full!
        progress = base_progress + max_progress;
        return true;
    }
}
