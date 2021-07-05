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

#pragma once

#include <common/types.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::loader {
    static constexpr int const ROFS_MODERN_VERSION = 0x200;

    #pragma pack(push, 1)
    struct rofs_header {
        char magic_[4];
        std::uint8_t header_size_;
        std::uint8_t reserved_;
        std::uint16_t rofs_format_version_;
        std::uint32_t dir_tree_offset_;
        std::uint32_t dir_tree_size_;
        std::uint32_t dir_file_entries_offset_;
        std::uint32_t dir_file_entries_size_;
        std::uint64_t time_;
        std::uint8_t major_;
        std::uint8_t minor_;
        std::uint16_t build_;
        std::uint32_t img_size_;
        std::uint32_t checksum_;
        std::uint32_t max_image_size_;
    };
    #pragma pack(pop)

    struct rofs_entry {
        std::uint16_t struct_size_;
        std::uint32_t uids_[3];         ///< Only if rofs_format_version >= ROFS_MODERN_VERSION
        std::uint32_t uid_check_;       ///< Only if rofs_format_version >= ROFS_MODERN_VERSION
        std::uint8_t name_offset_;      ///< Offset form the start of this entry. Probably for fast lookup in the code
        std::uint8_t att_;
        std::uint32_t file_size_;
        std::uint32_t file_addr_;
        std::uint8_t att_extra_;
        std::u16string filename_;

        bool read(common::ro_stream &stream, const int version);
    };

    struct rofs_dir {
        std::uint16_t struct_size_;
        std::uint8_t padding_;
        std::uint8_t first_entry_offset_;
        std::uint32_t file_block_addr_;
        std::uint32_t file_block_size_;
        std::vector<rofs_entry> subdirs_;

        bool read(common::ro_stream &stream, const int version);
    };

    bool dump_rofs_system(common::ro_stream &stream, const std::string &path, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);
}
