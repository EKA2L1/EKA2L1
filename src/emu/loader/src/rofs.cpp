/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <loader/rofs.h>
#include <common/buffer.h>
#include <common/log.h>

#include <common/path.h>
#include <common/cvt.h>

namespace eka2l1::loader {
    bool rofs_entry::read(common::ro_stream &stream) {
        const std::uint64_t start_pos = stream.tell();

        if (stream.read(&struct_size_, 2) != 2) {
            return false;
        }

        if (stream.read(uids_, sizeof(uids_)) != sizeof(uids_)) {
            return false;
        }

        if (stream.read(&uid_check_, 4) != 4) {
            return false;
        }

        if (stream.read(&name_offset_, 1) != 1) {
            return false;
        }

        if (stream.read(&att_, 1) != 1) {
            return false;
        }

        if (stream.read(&file_size_, 4) != 4) {
            return false;
        }

        if (stream.read(&file_addr_, 4) != 4) {
            return false;
        }

        if (stream.read(&att_extra_, 1) != 1) {
            return false;
        }

        std::uint8_t length = 0;
        if (stream.read(&length, 1) != 1) {
            return false;
        }

        filename_.resize(length);
        stream.seek(start_pos + name_offset_, common::seek_where::beg);

        if (stream.read(filename_.data(), length * sizeof(char16_t)) != length * sizeof(char16_t)) {
            return false;
        }

        // Seek past the struct
        stream.seek(start_pos + struct_size_, common::seek_where::beg);

        return true;
    }

    bool rofs_dir::read(common::ro_stream &stream) {
        const std::uint64_t original_pos = stream.tell();

        if (stream.read(&struct_size_, 2) != 2) {
            return false;
        }

        if (stream.read(&padding_, 1) != 1) {
            return false;
        }

        if (stream.read(&first_entry_offset_, 1) != 1) {
            return false;
        }

        if (stream.read(&file_block_addr_, 4) != 4) {
            return false;
        }

        if (stream.read(&file_block_size_, 4) != 4) {
            return false;
        }

        while (stream.tell() - original_pos < struct_size_) {
            rofs_entry subdir_entry;
            if (!subdir_entry.read(stream)) {
                return false;
            }

            subdirs_.push_back(std::move(subdir_entry));
        }

        return true;
    }

    static bool extract_file(common::ro_stream &stream, rofs_entry &entry, const std::string &base, std::atomic<int> &progress,
        const int base_progress, const int max_progress) {
        std::string fname = common::ucs2_to_utf8(entry.filename_);
        if (common::is_platform_case_sensitive()) {
            fname = common::lowercase_string(fname);
        }

        fname = add_path(base, fname);
        std::ofstream extract_stream(fname, std::ios_base::binary);

        const std::uint64_t org_pos = stream.tell();
        stream.seek(entry.file_addr_, common::seek_where::beg);

        static constexpr std::uint32_t CHUNK_SIZE = 0x10000;
        std::vector<char> buf;

        std::int64_t size_left = static_cast<std::int64_t>(entry.file_size_);
        while (size_left > 0) {
            const std::uint32_t size_to_take = common::min<std::uint32_t>(static_cast<std::uint32_t>(size_left),
                CHUNK_SIZE);

            buf.resize(size_to_take);

            const std::uint64_t amount_read = stream.read(buf.data(), size_to_take) ;
            if (amount_read < size_to_take) {
                LOG_WARN(LOADER, "Can't read {} bytes, skipping", size_to_take - amount_read);
            }

            progress = common::max(progress.load(), base_progress + static_cast<int>(stream.tell() * max_progress / stream.size()));

            extract_stream.write(buf.data(), size_to_take);
            size_left -= size_to_take;
        }

        stream.seek(org_pos, common::seek_where::beg);
        return true;
    }

    static bool extract_directory(common::ro_stream &stream, const std::string &base, const std::uint32_t offset,
        std::atomic<int> &progress, const int base_progress, const int max_progress) {
        eka2l1::create_directories(base);
        stream.seek(offset, common::seek_where::beg);

        // Read directory entry
        rofs_dir dir_var;
        if (!dir_var.read(stream)) {
            return false;
        }

        // Went through our files first
        if (dir_var.file_block_addr_) {
            stream.seek(dir_var.file_block_addr_, common::seek_where::beg);
            while (stream.tell() - dir_var.file_block_addr_ < dir_var.file_block_size_) {
                rofs_entry file_entry;
                if (!file_entry.read(stream)) {
                    return false;
                }

                if (!extract_file(stream, file_entry, base, progress, base_progress, max_progress)) {
                    LOG_ERROR(LOADER, "Fail to extract file with name: {}", common::ucs2_to_utf8(file_entry.filename_));
                }
            }
        }

        for (auto &subdir_ent: dir_var.subdirs_) {
            std::string subdir_name = common::ucs2_to_utf8(subdir_ent.filename_);
            if (common::is_platform_case_sensitive()) {
                subdir_name = common::lowercase_string(subdir_name);
            }

            if (!extract_directory(stream, eka2l1::add_path(base, subdir_name + eka2l1::get_separator()), subdir_ent.file_addr_,
                progress, base_progress, max_progress)) {
                return false;
            }
        }

        return true;
    }
    
    bool dump_rofs_system(common::ro_stream &stream, const std::string &path,std::atomic<int> &progress,
        const int max_progress) {
        rofs_header rheader;
        if (stream.read(&rheader, sizeof(rofs_header)) != sizeof(rofs_header)) {
            return false;
        }

        if (rheader.magic_[0] != 'R' || (rheader.magic_[1] != 'O') || (rheader.magic_[2] != 'F')
            || ((rheader.magic_[3] != 'S') && (rheader.magic_[3] != 'X'))) {
            return false;
        }

        if (rheader.magic_[3] == 'X') {
            LOG_WARN(LOADER, "Untested ROFX variant, please contact developer if wronggoings");
        }

        int base_progress = progress.load();
        const bool result = extract_directory(stream, path, rheader.dir_tree_offset_, progress,
            base_progress, max_progress);

        if (!result) {
            return false;
        }

        progress = base_progress + max_progress;
        return true;
    }
}