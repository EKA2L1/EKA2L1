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

#include <loader/sis_old.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/buffer.h>

#include <algorithm>
#include <cctype>
#include <cwctype>

#include <miniz.h>
#include <stack>

namespace eka2l1::loader {
    void sis_old_block::add_record(std::unique_ptr<sis_old_file_record> &record) {
        if (recording_mode == 0) {
            true_commands.push_back(std::move(record));
        } else {
            false_commands.push_back(std::move(record));
        }
    }

    std::unique_ptr<sis_old_expression> parse_sis_old_expression(common::ro_stream &stream) {
        std::uint32_t cond = 0;
        if (stream.read(&cond, 4) != 4) {
            return nullptr;
        }

        switch (cond) {
        case sis_old_file_expression_type_equal:
        case sis_old_file_expression_type_not_equal:
        case sis_old_file_expression_type_greater:
        case sis_old_file_expression_type_gequal:
        case sis_old_file_expression_type_less:
        case sis_old_file_expression_type_lequal:
        case sis_old_file_expression_type_and:
        case sis_old_file_expression_type_or:
        case sis_old_file_expression_type_appcap: {
            std::unique_ptr<sis_old_expression> lhs = parse_sis_old_expression(stream);
            std::unique_ptr<sis_old_expression> rhs = parse_sis_old_expression(stream);

            return std::make_unique<sis_old_binary_expression>(cond, lhs, rhs);
        }

        case sis_old_file_expression_type_exists:
        case sis_old_file_expression_type_devcap:
        case sis_old_file_expression_type_not: {
            std::unique_ptr<sis_old_expression> single = parse_sis_old_expression(stream);
            return std::make_unique<sis_old_unary_expression>(cond, single);
        }

        case sis_old_file_expression_type_number: {
            std::uint32_t value = 0;
            if (stream.read(&value, 4) != 4) {
                return nullptr;
            }
            // Padding
            stream.seek(4, common::seek_where::cur);
            return std::make_unique<sis_old_number_expression>(value);
        }
        
        case sis_old_file_expression_type_attribute: {
            std::uint32_t attrib = 0;
            if (stream.read(&attrib, 4) != 4) {
                return nullptr;
            }
            // Padding
            stream.seek(4, common::seek_where::cur);
            return std::make_unique<sis_old_attribute_expression>(attrib);
        }

        case sis_old_file_expression_type_string: {
            std::uint32_t length = 0;
            std::uint32_t ptr = 0;
            if (stream.read(&length, 4) != 4) {
                return nullptr;
            }

            if (stream.read(&ptr, 4) != 4) {
                return nullptr;
            }

            const std::uint64_t crr = stream.tell();

            stream.seek(ptr, common::seek_where::beg);

            std::u16string target_string((length + 1) / 2, u'\0');
            if (stream.read(target_string.data(), length) != length) {
                return nullptr;
            }

            stream.seek(crr, common::seek_where::beg);

            return std::make_unique<sis_old_string_expression>(target_string);
        }

        default:
            LOG_ERROR(PACKAGE, "Unrecognised if/else expression type {}!", cond);
            break;
        }

        return nullptr;
    }
    std::optional<sis_old> parse_sis_old(const std::string &path) {
        common::ro_std_file_stream stream(path, true);

        if (!stream.valid()) {
            return std::nullopt;
        }

        sis_old sold; // Not like trading anything

        if (stream.read(&sold.header, sizeof(sis_old_header)) != sizeof(sis_old_header)) {
            return std::nullopt;
        }

        sold.epoc_ver = (sold.header.uid2 == static_cast<uint32_t>(epoc_sis_type::epocu6)) ? epocver::epocu6 : epocver::epoc6;
        stream.seek(sold.header.file_ptr, common::seek_where::beg);

        std::stack<sis_old_block*> active_blocks;
        active_blocks.push(&sold.root_block);

        for (uint32_t i = 0; i < sold.header.num_files; i++) {
            std::uint32_t file_record_type = 0;

            if (stream.read(&file_record_type, 4) != 4) {
                return std::nullopt;
            }

            if (file_record_type <= file_record_type_multiple_lang_file) {
                sis_old_file old_file;
                old_file.file_record_type = file_record_type;

                if (stream.read(&old_file.file_type, 4) != 4) {
                    return std::nullopt;
                }

                if (stream.read(&old_file.file_details, 4) != 4) {
                    return std::nullopt;
                }

                std::uint32_t source_name_length = 0;
                std::uint32_t source_name_ptr = 0;

                if (stream.read(&source_name_length, 4) != 4) {
                    return std::nullopt;
                }

                if (stream.read(&source_name_ptr, 4) != 4) {
                    return std::nullopt;
                }

                std::size_t crr = stream.tell();

                stream.seek(source_name_ptr, common::seek_where::beg);
                old_file.name.resize(source_name_length / 2);

                // Unused actually, we currently don't care
                [[maybe_unused]] std::size_t total_size_read = 0;
                total_size_read = stream.read(old_file.name.data(), source_name_length);

                stream.seek(crr, common::seek_where::beg);

                std::uint32_t dest_name_length = 0;
                std::uint32_t dest_name_ptr = 0;

                if (stream.read(&dest_name_length, 4) != 4) {
                    return std::nullopt;
                }

                if (stream.read(&dest_name_ptr, 4) != 4) {
                    return std::nullopt;
                }

                crr = stream.tell();

                stream.seek(dest_name_ptr, common::seek_where::beg);
                old_file.dest.resize(dest_name_length / 2);

                total_size_read = stream.read(old_file.dest.data(), dest_name_length);
                stream.seek(crr, common::seek_where::beg);

                if (file_record_type == file_record_type_multiple_lang_file) {
                    old_file.file_infos.resize(sold.header.num_langs);
                } else {
                    old_file.file_infos.resize(1);
                }
                for (std::size_t i = 0; i < old_file.file_infos.size(); i++) {
                    if (stream.read(&old_file.file_infos[i].length, 4) != 4) {
                        return std::nullopt;
                    }

                    old_file.file_infos[i].original_length = old_file.file_infos[i].length;
                }

                for (std::size_t i = 0; i < old_file.file_infos.size(); i++) {
                    if (stream.read(&old_file.file_infos[i].position, 4) != 4) {
                        return std::nullopt;
                    }
                }

                if (sold.epoc_ver >= epocver::epoc6) {
                    for (std::size_t i = 0; i < old_file.file_infos.size(); i++) {
                        if (stream.read(&old_file.file_infos[i].original_length, 4) != 4) {
                            return std::nullopt;
                        }
                    }

                    std::uint32_t mime_name_length = 0;
                    std::uint32_t mime_name_ptr = 0;

                    if (stream.read(&mime_name_length, 4) != 4) {
                        return std::nullopt;
                    }

                    if (stream.read(&mime_name_ptr, 4) != 4) {
                        return std::nullopt;
                    }

                    crr = stream.tell();

                    stream.seek(mime_name_ptr, common::seek_where::beg);
                    old_file.mime.resize(mime_name_length / 2);

                    total_size_read = stream.read(old_file.mime.data(), mime_name_length);
                    stream.seek(crr, common::seek_where::beg);
                }

                std::unique_ptr<sis_old_file_record> record = std::make_unique<sis_old_file>(old_file);
                active_blocks.top()->add_record(record);
            } else if ((file_record_type == file_record_type_if) || (file_record_type == file_record_type_elseif)) {
                std::uint32_t cond_expression_size = 0;
                if (stream.read(&cond_expression_size, 4) != 4) {
                    return std::nullopt;
                }

                if (file_record_type == file_record_type_if) {
                    if (active_blocks.top()->recording_mode == 1)
                        LOG_WARN(PACKAGE, "Trying to add IF command when ELSE command is already called!");
                    else
                        active_blocks.top()->recording_mode = 0;
                }
                
                if (file_record_type == file_record_type_elseif) {
                    active_blocks.top()->recording_mode = 1;
                }

                const std::uint64_t crr = stream.tell();

                std::unique_ptr<sis_old_file_record> new_if_block = std::make_unique<sis_old_block>();
                sis_old_block *new_if_block_raw_ptr = reinterpret_cast<sis_old_block*>(new_if_block.get());
                new_if_block_raw_ptr->condition = parse_sis_old_expression(stream);

                if ((stream.tell() - crr) != cond_expression_size) {
                    LOG_WARN(PACKAGE, "Parsed conditional expression type consumed read size does not match! (consumed={}, ideal={})",
                        stream.tell() - crr, cond_expression_size);
                }

                active_blocks.top()->add_record(new_if_block);
                active_blocks.push(new_if_block_raw_ptr);
            } else if (file_record_type == file_record_type_else) {
                if (active_blocks.size() <= 1) {
                    LOG_WARN(PACKAGE, "Trying to add an ELSE when there's no active IF command!");
                } else {
                    if (active_blocks.top()->recording_mode == 1) {
                        LOG_INFO(PACKAGE, "The active block already use ELSE command once, the next commands will be merged to previous block!");
                    }

                    active_blocks.top()->recording_mode = 1;
                }
            } else if (file_record_type == file_record_type_endif) {
                if (active_blocks.size() <= 1) {
                    LOG_ERROR(PACKAGE, "If/endif expression in the SIS package mismatched. Still proceed!");
                } else {
                    active_blocks.pop();
                }
            } else {
                LOG_ERROR(PACKAGE, "Unrecognised file record type {}", file_record_type);
            }
        }

        for (std::uint32_t i = 0; i < sold.header.num_langs; i++) {
            // Read the language
            std::uint16_t lang = 0;

            stream.seek(sold.header.lang_ptr + i * 2, common::seek_where::beg);
            stream.read(&lang, 2);

            sold.langs.push_back(static_cast<language>(lang));

            // Read component name
            std::u16string name;

            stream.seek(sold.header.comp_name_ptr + i * 4, common::seek_where::beg);

            // Read only one name
            std::uint32_t name_len = 0;
            stream.read(&name_len, 4);

            name.resize(name_len / 2);

            // Read name offset
            stream.seek(sold.header.comp_name_ptr + 4 * sold.header.num_langs + i * 4, common::seek_where::beg);
            std::uint32_t pkg_name_offset = 0;

            stream.read(&pkg_name_offset, 4);
            stream.seek(pkg_name_offset, common::seek_where::beg);
            stream.read(&name[0], name_len);

            sold.comp_names.push_back(name);
        }

        return sold;
    }
}
