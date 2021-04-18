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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::loader::firmware {
    enum fpsx_type {
        FPSX_TYPE_UDA = 0,
        FPSX_TYPE_ROFS = 1,
        FPSX_TYPE_ROFX = 2,
        FPSX_TYPE_CORE = 3
    };

    enum content_type : std::uint8_t {
        CONTENT_TYPE_CODE = 84,
        CONTENT_TYPE_UNK1 = 86,
        CONTENT_TYPE_DATA_CERT = 93
    };

    enum block_type : std::uint8_t {
        BLOCK_TYPE_BINARY = 0x17,
        BLOCK_TYPE_ROFS_HASH = 0x27,
        BLOCK_TYPE_CORE_CERT = 0x28,
        BLOCK_TYPE_H2E = 0x2E,
        BLOCK_TYPE_H30 = 0x30,
        BLOCK_TYPE_H3A = 0x3A,
        BLOCK_TYPE_H49 = 0x49
    };

    enum tlv_entry_type: std::uint8_t {
        TLV_ENTRY_MORE_ROOT_KEY_HASH_MORE = 13,
        TLV_ENTRY_ERASE_AREA_BB5 = 18,
        TLV_ENTRY_ONENAND_SUBTYPE_UNK2 = 19,
        TLV_ENTRY_FORMAT_PARITION_BB5 = 25,
        TLV_ENTRY_PARTITION_INFO_BB5 = 47,
        TLV_ENTRY_CMT_TYPE = 194,
        TLV_ENTRY_CMT_ALGO = 195,
        TLV_ENTRY_ERASE_DCT5 = 200,
        TLV_ENTRY_UNKC9 = 201,
        TLV_ENTRY_SECONDARY_SENDING_SPEED = 205,
        TLV_ENTRY_ALGO_SENDING_SPEED = 206,
        TLV_ENTRY_PROGRAM_SENDING_SPEED = 207,
        TLV_ENTRY_MESSAGE_SENDING_SPEED = 209,
        TLV_ENTRY_CMT_SUPPORTED_HW = 212,
        TLV_ENTRY_APE_SUPPORTED_HW = 225,
        TLV_ENTRY_UNKE4_IMPL = 228,
        TLV_ENTRY_UNKE5 = 229,
        TLV_ENTRY_DATE_TIME = 230,
        TLV_ENTRY_APE_PHONE_TYPE = 231,
        TLV_ENTRY_APE_ALGORITHM = 232,
        TLV_ENTRY_UNKEA = 234,
        TLV_ENTRY_UNKEC = 236,
        TLV_ENTRY_UNKED = 237,
        TLV_ENTRY_ARRAY = 238,
        TLV_ENTRY_UNKF3_IMPL = 243,
        TLV_ENTRY_DESCR = 244,
        TLV_ENTRY_UNKF6 = 246,
        TLV_ENTRY_UNKF7 = 247,
        TLV_ENTRY_UNKFA_IMPL = 250
    };

    enum asic_type : std::uint8_t {
        ASIC_TYPE_CMT = 0,
        ASIC_TYPE_APE = 1,
        ASIC_TYPE_BOTH = 2
    };

    enum device_type : std::uint8_t {
        DEVICE_TYPE_NOR = 0,
        DEVICE_TYPE_NAND = 1,
        DEVICE_TYPE_MUX_ONE_NAND = 3,
        DEVICE_TYPE_MMC = 4,
        DEVICE_TYPE_RAM = 5
    };

    struct tlv_entry {
        tlv_entry_type type_;
        std::uint16_t size_;

        std::vector<std::uint8_t> content_;

        explicit tlv_entry(const tlv_entry_type type)
            : type_(type) {
        }

        virtual bool read(common::ro_stream &stream);
    };

    struct tlv_entry_large: public tlv_entry {
        explicit tlv_entry_large(const tlv_entry_type type)
            : tlv_entry(type) {
        }

        bool read(common::ro_stream &stream) override;
    };

    using tlv_entry_instance = std::unique_ptr<tlv_entry>;

    struct tlv_entry_area_bb5_range {
        std::uint32_t start_;
        std::uint32_t end_;
    };

    struct tlv_entry_erase_area_bb5: public tlv_entry {
        asic_type asic_type_;
        device_type device_type_;

        std::vector<tlv_entry_area_bb5_range> ranges_;

        explicit tlv_entry_erase_area_bb5()
            : tlv_entry(TLV_ENTRY_ERASE_AREA_BB5) {
        }

        bool read(common::ro_stream &stream) override;
    };

    struct tlv_entry_string_collection: public tlv_entry {
        std::vector<std::string> strs_;

        explicit tlv_entry_string_collection(const tlv_entry_type type)
            : tlv_entry(type) {
        }

        bool read(common::ro_stream &stream) override;
    };

    struct tlv_entry_time_since_epoch: public tlv_entry {
        std::uint32_t second_since_epoch_;

        explicit tlv_entry_time_since_epoch()
            : tlv_entry(TLV_ENTRY_DATE_TIME) {
        }

        bool read(common::ro_stream &stream) override;
    };

    struct tlv_entry_array: public tlv_entry_large {
        std::vector<tlv_entry_instance> elements_;

        explicit tlv_entry_array()
            : tlv_entry_large(TLV_ENTRY_ARRAY) {
        }

        bool read(common::ro_stream &stream) override;
    };

    struct block_header {
        std::uint8_t size_;

        std::uint32_t data_offset_to_extract_;
        std::uint32_t data_size_;

        virtual bool read(common::ro_stream &stream);
    };

    struct block_header_binary: public block_header {
        asic_type flash_memory_;
        std::uint16_t unk2_;
        std::uint16_t data_checksum_;
        std::uint8_t padding_;

        bool read(common::ro_stream &stream) override;
    };

    struct block_header_rofs_hash: public block_header {
        std::uint8_t cmt_root_key_hash_sha1_[20];
        char description_[12];
        asic_type flash_memory_;
        std::uint16_t unk2_;
        std::uint16_t data_checksum_;

        bool read(common::ro_stream &stream) override;
    };

    struct block_header_core_cert: public block_header_rofs_hash {
        std::uint8_t rap_papub_keys_maybe_hash_sha1_[20];
        std::uint16_t unk2b_;

        bool read(common::ro_stream &stream) override;
    };

    struct block_header_h2e: public block_header {
        asic_type flash_memory_;
        std::uint16_t unk2_;
        std::uint16_t data_checksum_;
        char description_[12];

        bool read(common::ro_stream &stream) override;
    };

    struct block_header_h30: public block_header {
        asic_type flash_memory_;
        std::uint16_t unk2_;
        std::uint16_t data_checksum_;
        char unk3_[8];

        bool read(common::ro_stream &stream) override;
    };

    struct block_header_h3a: public block_header {
        asic_type flash_memory_;
        std::uint16_t unk2_;
        std::uint16_t data_checksum_;
        char description_[12];

        bool read(common::ro_stream &stream) override;
    };

    struct block_header_h49: public block_header {
        std::uint16_t data_checksum_;
        char unk_[18];

        bool read(common::ro_stream &stream) override;
    };

    struct block {
        content_type ctype_;
        std::uint8_t padding_;
        block_type btype_;
        std::uint8_t block_checksum8_;
        std::size_t data_offset_in_stream_;

        std::unique_ptr<block_header> header_;
    };

    struct block_tree_entry {
        block myself_;

        std::vector<block> code_blocks_;
        std::vector<block> cert_blocks_;
        std::vector<block> unk56_blocks_;

        block *add(block &blck);
    };

    struct block_tree {
        std::vector<block_tree_entry> roots_;
        block *current_block_;

        explicit block_tree()
            : current_block_(nullptr) {
        }

        bool add(block &blck);
    };

    struct fpsx_header {
        std::uint8_t magic_;
        std::uint32_t header_size_;
        
        fpsx_type type_;

        std::vector<tlv_entry_instance> tlv_entries_;
        block_tree btree_;
    };

    std::optional<fpsx_header> read_fpsx_header(common::ro_stream &stream);
}
