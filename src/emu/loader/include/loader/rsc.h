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
#include <string>
#include <vector>

namespace eka2l1 {
    namespace common {
        class ro_stream;
        class chunkyseri;
    }
}

namespace eka2l1::loader {
    class rsc_file_impl_base {
    public:
        virtual ~rsc_file_impl_base() {
        }

        virtual std::vector<std::uint8_t> read(const int res_id) = 0;
        virtual std::uint16_t get_total_resources() const = 0;

        virtual std::uint32_t get_uid(const int idx) {
            return 0;
        }

        virtual bool confirm_signature() {
            return true;
        }
    };

    class rsc_file_legacy: public rsc_file_impl_base {
    private:
        enum file_type {
            file_type_non_unicode = 0,
            file_type_unicode = 1,
            file_type_compressed = 2
        } type_;

        std::int16_t resource_count_;
        std::int16_t resource_index_section_offset_;
        std::int16_t lookup_table_end_;
        std::int16_t max_resource_size_;
        std::int8_t  lookup_table_read_bit_count_;

        std::vector<std::uint8_t> res_data_;
        std::vector<std::int16_t> res_data_offset_table_;
        std::vector<std::uint16_t> lookup_offset_table_;
        std::vector<std::uint8_t> lookup_data_;

        bool lookup_mode_;

        bool read_header(common::ro_stream *seri);
        std::optional<std::uint16_t> read_bits(const std::int16_t offset, const std::int16_t count);

    public:
        explicit rsc_file_legacy(common::ro_stream *seri);
        ~rsc_file_legacy() override;

        void read_internal(const int res_id, std::vector<std::uint8_t> &buffer, const bool is_lookup = false);
        std::vector<std::uint8_t> read(const int res_id) override;

        std::uint16_t get_total_resources() const override {
            return resource_count_;
        }
    };

    class rsc_file_morden : public rsc_file_impl_base {
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
        bool does_resource_contain_unicode(int res_id, bool first_rsc_is_gen);
        int decompress(std::uint8_t *buffer, int max, int res_index);

        bool own_res_id(const int res_id);

    public:
        explicit rsc_file_morden(common::ro_stream *seri);
        ~rsc_file_morden() override;

        std::vector<std::uint8_t> read(const int res_id) override;
        std::uint32_t get_uid(const int idx) override;

        std::uint16_t get_total_resources() const override {
            return num_res;
        }

        bool confirm_signature() override;
    };

    class rsc_file {
    protected:
        std::unique_ptr<rsc_file_impl_base> impl_;
        void instantiate_impl(common::ro_stream *stream);

    public:
        explicit rsc_file(common::ro_stream *stream);
        
        std::vector<std::uint8_t> read(const int res_id);
        std::uint32_t get_uid(const int idx);
        std::uint16_t get_total_resources() const;
        bool confirm_signature();
    };

    void absorb_resource_string(common::chunkyseri &seri, std::u16string &str);
}
