/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <common/buffer.h>

namespace eka2l1 {
    struct file;
    using symfile = std::shared_ptr<file>;
}

namespace eka2l1::loader {
    class rsc_file_read_stream {
        std::vector<std::uint8_t> buffer;
        common::ro_buf_stream stream;

        std::uint8_t num_of_bits_use_for_dict_token = 0;
        std::uint16_t num_res = 0;

        int     num_dir_entry = 0;

        std::uint16_t dict_offset;
        std::uint16_t dict_index_offset;
        std::uint16_t res_offset;
        std::uint16_t res_index_offset;

        enum {
            potentially_have_compressed_unicode_data = 0x1000,
            dictionary_compressed = 0x2000,
            calypso = 0x40000,
            third_uid_offset = 0x8000,
            generate_rss_sig_for_first_user_res = 0x10000,
            first_res_generated_bit_array_of_res_contains_compressed_unicode = 0x20000
        };

        std::uint32_t flags;
        std::uint16_t size_of_largest_resource_when_uncompressed;

        std::vector<std::uint8_t> unicode_flag_array;
        std::vector<std::uint8_t> res_data;

        std::vector<std::uint16_t> resource_offsets;
        std::vector<std::uint16_t> dict_offsets;

    protected:
        bool do_decompress(symfile f);
        void read_header_and_resource_index(symfile f);

        bool is_resource_contains_unicode(int res_id, bool first_rsc_is_gen);
        int decompress(symfile f, std::uint8_t *buffer, int max, int res_index);

    public:
        explicit rsc_file_read_stream(symfile f, bool *success = nullptr);

        template <typename T>
        std::optional<T> read();

        /*! \brief Read to the specified buffer with given length. 
		 *
		 *  \returns False if stream is end, fail.
		 */
        bool read_unsafe(std::uint8_t *buffer, const std::size_t len);

        void rewind(const std::size_t len);

        void advance(const std::size_t len);
    };
}