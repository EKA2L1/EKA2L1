/*
 * Copyright (c) 2020 EKA2L1 Team.
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

// This old SIS installer and its code is based on this document from Mr. Thoukydides.
// Thank you very much! The link to this document can be found here:
// https://thoukydides.github.io/riscos-psifs/sis.html

#pragma once

#include <common/types.h>
#include <loader/sis_common.h>

#include <cstdint>
#include <optional>
#include <memory>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace loader {
        // Only the app type will be remembered by
        // the app manager
        enum class sis_old_type {
            app,
            sys,
            optional,
            config,
            patch,
            upgrade
        };

        enum sis_old_file_record_type {
            file_record_type_simple_file,
            file_record_type_multiple_lang_file,
            file_record_type_options,
            file_record_type_if,
            file_record_type_elseif,
            file_record_type_else,
            file_record_type_endif,
            // These types are emulator's
            file_record_type_expr,
            file_record_type_block
        };

        enum sis_old_file_expression_type {
            sis_old_file_expression_type_equal,
            sis_old_file_expression_type_not_equal,
            sis_old_file_expression_type_greater,
            sis_old_file_expression_type_less,
            sis_old_file_expression_type_gequal,
            sis_old_file_expression_type_lequal,
            sis_old_file_expression_type_and,
            sis_old_file_expression_type_or,
            sis_old_file_expression_type_exists,
            sis_old_file_expression_type_devcap,
            sis_old_file_expression_type_appcap,
            sis_old_file_expression_type_not,
            sis_old_file_expression_type_string,
            sis_old_file_expression_type_attribute,
            sis_old_file_expression_type_number
        };

        struct sis_old_header {
            uint32_t uid1;
            uint32_t uid2;
            uint32_t uid3;
            uint32_t uid4;
            uint16_t checksum;
            uint16_t num_langs;
            uint16_t num_files;
            uint16_t num_reqs;
            uint16_t lang;
            uint16_t install_file;
            uint16_t install_drive;
            uint16_t num_caps;
            uint32_t install_ver;
            uint16_t op;
            uint16_t type;
            uint16_t major;
            uint16_t minor;
            uint32_t variant;
            uint32_t lang_ptr;
            uint32_t file_ptr;
            uint32_t req_ptr;
            uint32_t cert_ptr;
            uint32_t comp_name_ptr;
        };

        enum class sis_old_file_type {
            standard,
            text,
            comp,
            run,
            null,
            mime
        };

        enum class sis_old_file_detail {
            cont,
            skip,
            abort,
            undo
        };

        enum class sis_old_file_detail_exec {
            install,
            remove,
            both,
            sended
        };

        struct sis_old_file_record {
            std::uint32_t file_record_type;
        };

        struct sis_old_file : public sis_old_file_record {
            std::uint32_t file_type;
            std::uint32_t file_details;
            std::uint32_t mime_type_len;
            std::uint32_t mine_type_ptr;
            
            struct individual_file_data_info {
                std::uint32_t length;
                std::uint32_t position;
                std::uint32_t original_length;
            };

            std::vector<individual_file_data_info> file_infos;

            std::u16string name;
            std::u16string dest;
            std::u16string mime;
        };

        struct sis_old_expression : public sis_old_file_record {
            std::uint32_t expression_type;

            explicit sis_old_expression(const std::uint32_t expr_type)
                : expression_type(expr_type) {
                file_record_type = file_record_type_expr;
            }
        };

        struct sis_old_unary_expression : public sis_old_expression {
            std::unique_ptr<sis_old_expression> target_expression;

            explicit sis_old_unary_expression(const std::uint32_t expr_type, std::unique_ptr<sis_old_expression> &expr)
                : sis_old_expression(expr_type)
                , target_expression(std::move(expr)) {
            }
        };

        struct sis_old_binary_expression : public sis_old_expression {
            std::unique_ptr<sis_old_expression> lhs;
            std::unique_ptr<sis_old_expression> rhs;
            
            explicit sis_old_binary_expression(const std::uint32_t expr_type, std::unique_ptr<sis_old_expression> &alhs,
                std::unique_ptr<sis_old_expression> &arhs)
                : sis_old_expression(expr_type)
                , lhs(std::move(alhs))
                , rhs(std::move(arhs)) {
            }
        };

        struct sis_old_string_expression : public sis_old_expression {
            std::u16string value;

            explicit sis_old_string_expression(const std::u16string &mstring)
                : sis_old_expression(sis_old_file_expression_type_string)
                , value(mstring) {
            }
        };

        struct sis_old_number_expression : public sis_old_expression {
            std::uint32_t value;

            explicit sis_old_number_expression(const std::uint32_t value)
                : sis_old_expression(sis_old_file_expression_type_number)
                , value(value) {
            }
        };

        struct sis_old_attribute_expression : public sis_old_expression {
            std::uint32_t attribute;

            explicit sis_old_attribute_expression(const std::uint32_t value)
                : sis_old_expression(sis_old_file_expression_type_attribute)
                , attribute(value) {
            }
        };

        struct sis_old_block : public sis_old_file_record {
            std::unique_ptr<sis_old_expression> condition;
            std::vector<std::unique_ptr<sis_old_file_record>> true_commands;
            std::vector<std::unique_ptr<sis_old_file_record>> false_commands;

            std::uint32_t recording_mode;

            explicit sis_old_block() {
                file_record_type = file_record_type_block;
                recording_mode = 0;
            }

            void add_record(std::unique_ptr<sis_old_file_record> &record);
        };

        struct sis_old {
            sis_old_header header;
            sis_old_block root_block;

            epocver epoc_ver;
            std::vector<std::u16string> comp_names;
            std::vector<language> langs;
        };

        std::optional<sis_old> parse_sis_old(const std::string &path);
    }
}
