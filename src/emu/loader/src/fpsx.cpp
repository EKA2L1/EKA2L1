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

#include <loader/fpsx.h>
#include <common/buffer.h>

#include <common/log.h>
#include <common/bytes.h>

namespace eka2l1::loader::firmware {
    bool tlv_entry::read(common::ro_stream &stream) {
        std::uint8_t size_temp = 0;
        if (stream.read(&size_temp, 1) != 1) {
            return false;
        }

        size_ = size_temp;

        content_.resize(size_);
        if (stream.read(content_.data(), size_) != size_) {
            return false;
        }

        return true;
    }

    bool tlv_entry_large::read(common::ro_stream &stream) {
        if (stream.read(&size_, 2) != 2) {
            return false;
        }

        if (common::get_system_endian_type() == common::little_endian) {
            size_ = common::byte_swap(size_);
        }

        content_.resize(size_);
        if (stream.read(content_.data(), size_) != size_) {
            return false;
        }

        return true;
    }

    bool tlv_entry_erase_area_bb5::read(common::ro_stream &stream) {
        if (!tlv_entry::read(stream)) {
            return false;
        }

        common::ro_buf_stream head_stream(content_.data(), content_.size());
        if (head_stream.read(&asic_type_, 1) != 1) {
            return false;
        }

        if (head_stream.read(&device_type_, 1) != 1) {
            return false;
        }

        std::uint8_t padding = 0;
        if (head_stream.read(&padding, 1) != 1) {
            return false;
        }

        const std::size_t range_count = (content_.size() - 3) / sizeof(tlv_entry_area_bb5_range);
        for (std::size_t i = 0; i < range_count; i++) {
            tlv_entry_area_bb5_range range;
            if (head_stream.read(&range.start_, 4) != 4) {
                return false;
            }

            if (head_stream.read(&range.end_, 4 != 4)) {
                return false;
            }

            if (common::get_system_endian_type() == common::little_endian) {
                range.start_ = common::byte_swap(range.start_);
                range.end_ = common::byte_swap(range.end_);
            }

            ranges_.push_back(range);
        }

        return true;
    }

    bool tlv_entry_string_collection::read(common::ro_stream &stream) {
        if (!tlv_entry::read(stream)) {
            return false;
        }

        std::string current;

        for (std::size_t i = 0; i < content_.size(); i++) {
            if (content_[i] == '\0') {
                strs_.push_back(current);
                current.clear();
            } else {
                current += static_cast<char>(content_[i]);
            }
        }

        return true;
    }

    bool tlv_entry_time_since_epoch::read(common::ro_stream &stream) {
        if (!tlv_entry::read(stream)) {
            return false;
        }

        second_since_epoch_ = *reinterpret_cast<std::uint32_t*>(content_.data());
        if (common::get_system_endian_type() == common::little_endian) {
            second_since_epoch_ = common::byte_swap(second_since_epoch_);
        }

        return true;
    }
    
    static bool read_tlv_entries(common::ro_stream &stream, std::vector<tlv_entry_instance> &entries);

    bool tlv_entry_array::read(common::ro_stream &stream) {
        if (!tlv_entry_large::read(stream)) {
            return false;
        }

        common::ro_buf_stream head_stream(content_.data(), content_.size());
        std::uint8_t entry_count = 0;

        if (head_stream.read(&entry_count, 1) != 1) {
            return false;
        }

        elements_.resize(entry_count);
        return read_tlv_entries(head_stream, elements_);
    }

    static bool read_tlv_entries(common::ro_stream &stream, std::vector<tlv_entry_instance> &entries) {
        for (std::size_t i = 0; i < entries.size(); i++) {
            tlv_entry_type type = TLV_ENTRY_UNKFA_IMPL;
            if (stream.read(&type, 1) != 1) {
                return false;
            }

            tlv_entry_instance tlv_inst = nullptr;
            switch (type) {
            case TLV_ENTRY_ERASE_AREA_BB5:
                tlv_inst = std::make_unique<tlv_entry_erase_area_bb5>();
                break;

            case TLV_ENTRY_CMT_TYPE:
            case TLV_ENTRY_CMT_ALGO:
            case TLV_ENTRY_DESCR:
                tlv_inst = std::make_unique<tlv_entry_string_collection>(type);
                break;

            case TLV_ENTRY_DATE_TIME:
                tlv_inst = std::make_unique<tlv_entry_time_since_epoch>();
                break;

            case TLV_ENTRY_ARRAY:
                tlv_inst = std::make_unique<tlv_entry_array>();
                break;

            case TLV_ENTRY_UNKE4_IMPL:
            case TLV_ENTRY_UNKE5:
            case TLV_ENTRY_UNKEA:
            case TLV_ENTRY_UNKF3_IMPL:
                tlv_inst = std::make_unique<tlv_entry_large>(type);
                break;

            default:
                tlv_inst = std::make_unique<tlv_entry>(type);
                break;
            }

            if (!tlv_inst->read(stream)) {
                return false;
            }

            entries[i] = std::move(tlv_inst);
        }

        return true;
    }

    bool block_header::read(common::ro_stream &stream) {
        return (stream.read(&size_, 1) == 1);
    }

    bool block_header_binary::read(common::ro_stream &stream) {
        if (!block_header::read(stream)) {
            return false;
        }

        if (stream.read(&flash_memory_, 1) != 1) {
            return false;
        }

        if (stream.read(&unk2_, 2) != 2) {
            return false;
        }

        if (stream.read(&data_checksum_, 2) != 2) {
            return false;
        }

        if ((stream.read(&padding_, 1) != 1) || (padding_ != 1)) {
            return false;
        }

        if (stream.read(&data_size_, 4) != 4) {
            return false;
        }

        if (stream.read(&data_offset_to_extract_, 4) != 4) {
            return false;
        }

        if (common::get_system_endian_type() == common::little_endian) {
            data_size_ = common::byte_swap(data_size_);
            data_offset_to_extract_ = common::byte_swap(data_offset_to_extract_);
        }

        return true;
    }

    bool block_header_rofs_hash::read(common::ro_stream &stream) {
        if (!block_header::read(stream)) {
            return false;
        }

        if (stream.read(cmt_root_key_hash_sha1_, sizeof(cmt_root_key_hash_sha1_)) != sizeof(cmt_root_key_hash_sha1_)) {
            return false;
        }

        if (stream.read(description_, sizeof(description_)) != sizeof(description_)) {
            return false;
        }

        if (stream.read(&flash_memory_, 1) != 1) {
            return false;
        }

        if (stream.read(&unk2_, 2) != 2) {
            return false;
        }

        if (stream.read(&data_checksum_, 2) != 2) {
            return false;
        }

        if (stream.read(&data_size_, 4) != 4) {
            return false;
        }

        if (stream.read(&data_offset_to_extract_, 4) != 4) {
            return false;
        }

        if (common::get_system_endian_type() == common::little_endian) {
            data_size_ = common::byte_swap(data_size_);
            data_offset_to_extract_ = common::byte_swap(data_offset_to_extract_);
        }

        return true;
    }

    bool block_header_core_cert::read(common::ro_stream &stream) {
        if (!block_header_rofs_hash::read(stream)) {
            return false;
        }

        if (stream.read(rap_papub_keys_maybe_hash_sha1_, sizeof(rap_papub_keys_maybe_hash_sha1_)) != sizeof(rap_papub_keys_maybe_hash_sha1_)) {
            return false;
        }

        if (stream.read(&unk2_, 2) != 2) {
            return false;
        }

        return true;
    }

    bool block_header_h2e::read(common::ro_stream &stream) {
        if (!block_header::read(stream)) {
            return false;
        }

        if (stream.read(&flash_memory_, 1) != 1) {
            return false;
        }

        if (stream.read(&unk2_, 2) != 2) {
            return false;
        }

        if (stream.read(&data_checksum_, 2) != 2) {
            return false;
        }

        if (stream.read(&description_, sizeof(description_)) != sizeof(description_)) {
            return false;
        }

        if (stream.read(&data_size_, 4) != 4) {
            return false;
        }

        if (stream.read(&data_offset_to_extract_, 4) != 4) {
            return false;
        }

        if (common::get_system_endian_type() == common::little_endian) {
            data_size_ = common::byte_swap(data_size_);
            data_offset_to_extract_ = common::byte_swap(data_offset_to_extract_);
        }

        return true;
    }

    bool block_header_h30::read(common::ro_stream &stream) {
        if (!block_header::read(stream)) {
            return false;
        }

        if (stream.read(&flash_memory_, 1) != 1) {
            return false;
        }

        if (stream.read(&unk2_, 2) != 2) {
            return false;
        }

        if (stream.read(&data_checksum_, 2) != 2) {
            return false;
        }

        if (stream.read(&unk3_, sizeof(unk3_)) != sizeof(unk3_)) {
            return false;
        }

        if (stream.read(&data_size_, 4) != 4) {
            return false;
        }

        if (stream.read(&data_offset_to_extract_, 4) != 4) {
            return false;
        }

        if (common::get_system_endian_type() == common::little_endian) {
            data_size_ = common::byte_swap(data_size_);
            data_offset_to_extract_ = common::byte_swap(data_offset_to_extract_);
        }

        return true;
    }

    bool block_header_h3a::read(common::ro_stream &stream) {
        if (!block_header::read(stream)) {
            return false;
        }

        if (stream.read(&flash_memory_, 1) != 1) {
            return false;
        }

        if (stream.read(&unk2_, 2) != 2) {
            return false;
        }

        if (unk2_ != 0x1C08) {
            LOG_ERROR(LOADER, "Unk2 BlockH3A != 0x1C08 (real value = 0x{:X})", unk2_);
            return false;
        }

        if (stream.read(&data_checksum_, 2) != 2) {
            return false;
        }

        if (stream.read(&description_, sizeof(description_)) != sizeof(description_)) {
            return false;
        }

        data_size_ = 0;
        data_offset_to_extract_ = 0;

        return true;
    }

    bool block_header_h49::read(common::ro_stream &stream) {
        if (!block_header::read(stream)) {
            return false;
        }

        if (stream.read(&data_size_, 4) != 4) {
            return false;
        }

        if (stream.read(&data_checksum_, 2) != 2) {
            return false;
        }

        if (stream.read(&unk_, sizeof(unk_)) != sizeof(unk_)) {
            return false;
        }

        if (stream.read(&data_offset_to_extract_, 4) != 4) {
            return false;
        }

        if (common::get_system_endian_type() == common::little_endian) {
            data_size_ = common::byte_swap(data_size_);
            data_offset_to_extract_ = common::byte_swap(data_offset_to_extract_);
        }

        return true;
    }

    block *block_tree_entry::add(block &blck) {
        switch (blck.ctype_) {
        case CONTENT_TYPE_CODE:
            code_blocks_.push_back(std::move(blck));
            return &(code_blocks_.back());

        case CONTENT_TYPE_DATA_CERT:
            cert_blocks_.push_back(std::move(blck));
            return &(cert_blocks_.back());

        case CONTENT_TYPE_UNK1:
            unk56_blocks_.push_back(std::move(blck));
            return &(unk56_blocks_.back());

        default:
            break;
        }

        return nullptr;
    }

    bool block_tree::add(block &blck) {
        if (!current_block_) {
            block_tree_entry entry;
            current_block_ = entry.add(blck);

            roots_.push_back(std::move(entry));

            return true;
        }

        if ((current_block_->ctype_ == CONTENT_TYPE_CODE) && (blck.ctype_ == CONTENT_TYPE_DATA_CERT)) {
            block_tree_entry entry;
            current_block_ = entry.add(blck);

            roots_.push_back(std::move(entry));

            return true;
        }

        if ((blck.header_->data_offset_to_extract_ >= current_block_->header_->data_offset_to_extract_) &&
            (blck.header_->data_offset_to_extract_ <= current_block_->header_->data_offset_to_extract_ + current_block_->header_->data_size_)) {
            block_tree_entry &entry_root_last = roots_.back();
            current_block_ = entry_root_last.add(blck);
            return true;
        }

        block_tree_entry entry;
        current_block_ = entry.add(blck);

        roots_.push_back(std::move(entry));

        return true;
    }

    static std::optional<block> read_next_block(common::ro_stream &stream) {
        block blck;
        if (stream.read(&blck.ctype_, 1) != 1) {
            return std::nullopt;
        }

        std::uint8_t padding = 0;
        if (stream.read(&padding, 1) != 1) {
            return std::nullopt;
        }

        if (stream.read(&blck.btype_, 1) != 1) {
            return std::nullopt;
        }

        const std::size_t crr_pos = stream.tell();

        switch (blck.btype_) {
        case BLOCK_TYPE_BINARY:
            blck.header_ = std::make_unique<block_header_binary>();
            break;

        case BLOCK_TYPE_ROFS_HASH:
            blck.header_ = std::make_unique<block_header_rofs_hash>();
            break;

        case BLOCK_TYPE_CORE_CERT:
            blck.header_ = std::make_unique<block_header_core_cert>();
            break;

        case BLOCK_TYPE_H2E:
            blck.header_ = std::make_unique<block_header_h2e>();
            break;

        case BLOCK_TYPE_H30:
            blck.header_ = std::make_unique<block_header_h30>();
            break;

        case BLOCK_TYPE_H3A:
            blck.header_ = std::make_unique<block_header_h3a>();
            break;

        case BLOCK_TYPE_H49:
            blck.header_ = std::make_unique<block_header_h49>();
            break;

        default:
            LOG_ERROR(LOADER, "Unrecognised block type {}", static_cast<int>(blck.btype_));
            return std::nullopt;
        }

        if (!blck.header_->read(stream)) {
            return std::nullopt;
        }

        // Skip the header size also
        stream.seek(crr_pos + blck.header_->size_ + 1, common::seek_where::beg);
        if (stream.read(&blck.block_checksum8_, 1) != 1) {
            return std::nullopt;
        }

        blck.data_offset_in_stream_ = stream.tell();
        stream.seek(common::align(blck.header_->data_size_, 512), common::seek_where::cur);
    
        return blck;
    }

    static void guess_fpsx_type_from_blocks(common::ro_stream &stream, fpsx_header &header) {
        if (header.btree_.roots_.empty()) {
            header.type_ = FPSX_TYPE_INVALID;
            return;
        }

        // Don't merge! Sometimes core cert is just behind ROFS cert lol
        for (std::size_t i = 0; i < header.btree_.roots_.size(); i++) {
            block_tree_entry &entry = header.btree_.roots_[i];
            for (std::size_t j = 0; j < entry.cert_blocks_.size(); j++) {
                if (entry.cert_blocks_[j].btype_ == BLOCK_TYPE_CORE_CERT) {
                    header.type_ = FPSX_TYPE_CORE;
                    return;
                }
            }
        }

        for (std::size_t i = 0; i < header.btree_.roots_.size(); i++) {
            block_tree_entry &entry = header.btree_.roots_[i];

            if ((entry.cert_blocks_.size() > 0) && (entry.cert_blocks_[0].btype_ == BLOCK_TYPE_ROFS_HASH)) {
                block_header_rofs_hash &rofs_header = static_cast<decltype(rofs_header)>(*entry.cert_blocks_[0].header_);
                std::string description = rofs_header.description_;

                if (description.find("ROFS") != std::string::npos) {
                    header.type_ = FPSX_TYPE_ROFS;
                    return;
                } else if (description.find("ROFX") != std::string::npos) {
                    header.type_ = FPSX_TYPE_ROFX;
                    return;
                }
            }
        }

        if ((header.btree_.roots_.size() == 1) && (header.btree_.roots_[0].cert_blocks_.empty())) {
            header.type_ = FPSX_TYPE_UDA;
        } else {
            header.type_ = FPSX_TYPE_INVALID;
        }
    }

    block_tree_entry *fpsx_header::find_block_with_description(const std::string &str) {
        for (std::size_t i = 0; i < btree_.roots_.size(); i++) {
            block_tree_entry &entry = btree_.roots_[i];

            if ((entry.cert_blocks_.size() > 0) && (entry.cert_blocks_[0].btype_ == BLOCK_TYPE_ROFS_HASH)) {
                block_header_rofs_hash &rofs_header = static_cast<decltype(rofs_header)>(*entry.cert_blocks_[0].header_);
                std::string description = rofs_header.description_;

                if (description.find(str) != std::string::npos) {
                    return &entry;
                }
            }
        }

        return nullptr;
    }

    std::optional<fpsx_header> read_fpsx_header(common::ro_stream &stream) {
        const common::endian_type etype = common::get_system_endian_type();

        fpsx_header header;
        if (stream.read(&header.magic_, 1) != 1) {
            return std::nullopt;
        }

        if ((header.magic_ < 176) && (header.magic_ > 178)) {
            LOG_ERROR(LOADER, "Invalid header magic number {}", header.magic_);
            return std::nullopt;
        }

        if (stream.read(&header.header_size_, 4) != 4) {
            return std::nullopt;
        }

        if (etype == common::little_endian) {
            header.header_size_ = common::byte_swap(header.header_size_);
        }

        std::uint32_t tlv_entry_num = 0;
        if (stream.read(&tlv_entry_num, 4) != 4) {
            return std::nullopt;
        }

        if (etype == common::little_endian) {
            tlv_entry_num = common::byte_swap(tlv_entry_num);
        }

        header.tlv_entries_.resize(tlv_entry_num);
        const std::size_t current_pos = stream.tell();

        if (!read_tlv_entries(stream, header.tlv_entries_)) {
            return std::nullopt;
        }

        const std::size_t readed = stream.tell() - current_pos;
        if (readed < header.header_size_) {
            LOG_TRACE(LOADER, "Unknown {} skipped", header.header_size_ - readed);
        }

        // Skip the header size and magic, the entry num is in TLV section
        stream.seek(header.header_size_ + 5, common::seek_where::beg);
        while (stream.valid()) {
            std::optional<block> blck = read_next_block(stream);
            if (!blck)
                break;
                
            if (!header.btree_.add(blck.value())) {
                return std::nullopt;
            }
        }

        guess_fpsx_type_from_blocks(stream, header);
        return header;
    }
}