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

namespace eka2l1 {
    namespace common {
        class ro_stream;
    }
}

namespace eka2l1::loader {
    class rsc_file {
        struct uid_type {
            std::uint32_t uid1;
            std::uint32_t uid2;
            std::uint32_t uid3;
        } uids;

        std::uint8_t num_of_bits_use_for_dict_token = 0;
        std::uint16_t num_res = 0;

        int num_dir_entry = 0;

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

        struct sig_record {
            std::uint32_t sig;
            std::uint32_t offset;
        } signature;

        std::vector<std::uint8_t> unicode_flag_array;
        std::vector<std::uint8_t> res_data;

        std::vector<std::uint16_t> resource_offsets;
        std::vector<std::uint16_t> dict_offsets;

    protected:
        void read_header_and_resource_index(common::ro_stream *seri);

        int decompress(std::uint8_t *buffer, int max, int res_index);

        bool own_res_id(const int res_id);

    public:
        explicit rsc_file(common::ro_stream *seri);
        bool is_resource_contains_unicode(int res_id, bool first_rsc_is_gen);

        std::vector<std::uint8_t> read(const int res_id);
        std::uint32_t get_uid(const int idx);

        std::uint16_t get_total_resources() const {
            return num_res;
        }

        bool confirm_signature();
    };
}
