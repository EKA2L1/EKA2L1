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
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/flate.h>
#include <common/log.h>
#include <common/path.h>
#include <common/time.h>
#include <common/types.h>

#include <config/config.h>
#include <vfs/vfs.h>

#include <loader/e32img.h>
#include <loader/sis.h>

#include <package/manager.h>
#include <package/sis_script_interpreter.h>

#include <miniz.h>

namespace eka2l1 {
    namespace loader {
        std::string get_install_path(const std::u16string &pseudo_path, drive_number drv) {
            std::u16string raw_path = pseudo_path;

            if (raw_path.substr(0, 2) == u"!:") {
                raw_path[0] = drive_to_char16(drv);
            }

            return common::ucs2_to_utf8(raw_path);
        }

        bool ss_interpreter::appprop(sis_uid uid, sis_property prop) {
            return false;
        }

        bool ss_interpreter::package(sis_uid uid) {
            if (mngr->installed(uid.uid)) {
                return true;
            }

            return false;
        }

        ss_interpreter::ss_interpreter(common::ro_stream *stream,
            io_system *io,
            manager::packages *mngr,
            sis_controller *main_controller,
            sis_data *inst_data,
            drive_number inst_drv)
            : main_controller(main_controller)
            , install_data(inst_data)
            , conf(nullptr)
            , extract_target_accumulated_size(0)
            , extract_target_decomped_size(0)
            , progress_changed_cb(nullptr)
            , install_drive(inst_drv)
            , data_stream(stream)
            , io(io)
            , mngr(mngr) {
        }

        std::vector<uint8_t> ss_interpreter::get_small_file_buf(uint32_t data_idx, uint16_t crr_blck_idx) {
            sis_file_data *data = reinterpret_cast<sis_file_data *>(
                reinterpret_cast<sis_data_unit *>(install_data->data_units.fields[crr_blck_idx].get())->data_unit.fields[data_idx].get());
            sis_compressed compressed = data->raw_data;

            std::uint64_t us = ((compressed.len_low) | (static_cast<uint64_t>(compressed.len_high) << 32)) - 12;

            compressed.compressed_data.resize(us);

            data_stream->seek(compressed.offset, common::seek_where::beg);
            data_stream->read(&compressed.compressed_data[0], us);

            if (compressed.algorithm == sis_compressed_algorithm::none) {
                return compressed.compressed_data;
            }

            compressed.uncompressed_data.resize(compressed.uncompressed_size);
            mz_stream stream;

            stream.zalloc = nullptr;
            stream.zfree = nullptr;

            if (inflateInit(&stream) != MZ_OK) {
                LOG_ERROR(PACKAGE, "Can not intialize inflate stream");
            }

            flate::inflate_data(&stream, compressed.compressed_data.data(),
                compressed.uncompressed_data.data(), static_cast<std::uint32_t>(us));

            inflateEnd(&stream);

            return compressed.uncompressed_data;
        }

        // Assuming this file is small since it's stored in std::vector
        // Directly write this
        void extract_file_with_buf(const std::string &path, std::vector<uint8_t> &data) {
            std::string rp = eka2l1::file_directory(path);
            eka2l1::create_directories(rp);

            LOG_INFO(PACKAGE, "Write to: {}", path);

            FILE *temp = fopen(path.c_str(), "wb");
            fwrite(data.data(), 1, data.size(), temp);

            fclose(temp);
        }

        bool ss_interpreter::extract_file(const std::string &path, const uint32_t idx, uint16_t crr_blck_idx) {
            std::string rp = eka2l1::file_directory(path);
            eka2l1::create_directories(rp);

            // Delete the file, starts over
            if (common::is_system_case_insensitive() && eka2l1::exists(path)) {
                if (!common::remove(path)) {
                    LOG_WARN(PACKAGE, "Unable to remove {} to extract new file", path);
                }
            }

            FILE *file = fopen(path.c_str(), "wb");

            sis_data_unit *data_unit = reinterpret_cast<sis_data_unit *>(install_data->data_units.fields[crr_blck_idx].get());
            sis_file_data *data = reinterpret_cast<sis_file_data *>(data_unit->data_unit.fields[idx].get());

            sis_compressed compressed = data->raw_data;

            std::uint64_t left = ((compressed.len_low) | (static_cast<std::uint64_t>(compressed.len_high) << 32)) - 12;
            data_stream->seek(compressed.offset, common::seek_where::beg);

            std::vector<unsigned char> temp_chunk;
            temp_chunk.resize(CHUNK_SIZE);

            std::vector<unsigned char> temp_inflated_chunk;
            temp_inflated_chunk.resize(CHUNK_MAX_INFLATED_SIZE);

            mz_stream stream;

            stream.zalloc = nullptr;
            stream.zfree = nullptr;

            if (compressed.algorithm == sis_compressed_algorithm::deflated) {
                if (inflateInit(&stream) != MZ_OK) {
                    LOG_ERROR(PACKAGE, "Can not intialize inflate stream");
                }
            }

            std::uint32_t total_inflated_size = 0;
            bool cancel_requested = false;

            while (left > 0) {
                if (cancel_cb && cancel_cb()) {
                    cancel_requested = true;
                    break;
                }

                std::fill(temp_chunk.begin(), temp_chunk.end(), 0);
                int grab = static_cast<int>(left < CHUNK_SIZE ? left : CHUNK_SIZE);

                data_stream->read(&temp_chunk[0], grab);

                if (!data_stream->valid()) {
                    LOG_ERROR(PACKAGE, "Stream fail, skipping this file, should report to developers.");
                    return false;
                }

                if (compressed.algorithm == sis_compressed_algorithm::deflated) {
                    uint32_t inflated_size = 0;

                    auto res = flate::inflate_data(&stream, temp_chunk.data(), temp_inflated_chunk.data(), grab, &inflated_size);

                    if (!res) {
                        LOG_ERROR(PACKAGE, "Decompress failed! Report to developers");
                        return false;
                    }

                    fwrite(temp_inflated_chunk.data(), 1, inflated_size, file);

                    total_inflated_size += inflated_size;
                    extract_target_decomped_size += inflated_size;
                } else {
                    fwrite(temp_chunk.data(), 1, grab, file);
                    extract_target_decomped_size += grab;
                }

                left -= grab;

                if (progress_changed_cb) {
                    if (extract_target_accumulated_size != 0) {
                        progress_changed_cb(extract_target_decomped_size, extract_target_accumulated_size);
                    } else {
                        progress_changed_cb(100, 100);
                    }
                }
            }

            if (compressed.algorithm == sis_compressed_algorithm::deflated) {
                if (!cancel_requested && (total_inflated_size != compressed.uncompressed_size)) {
                    LOG_ERROR(PACKAGE, "Sanity check failed: Total inflated size not equal to specified uncompress size "
                                       "in SISCompressed ({} vs {})!",
                        total_inflated_size, compressed.uncompressed_size);
                }

                inflateEnd(&stream);
            }

            fclose(file);

            if (cancel_requested) {
                common::remove(path);
                return false;
            }

            return true;
        }

        int ss_interpreter::gasp_true_form_of_integral_expression(const sis_expression &expr) {
            switch (expr.op) {
            case ss_expr_op::EPrimTypeVariable: {
                switch (expr.int_val) {
                // Language variable. We choosen upper
                case 0x1000: {
                    return static_cast<int>(current_controllers.top()->chosen_lang);
                }

                default:
                    break;
                }

                if (var_resolver) {
                    return var_resolver(expr.int_val);
                }

                return 0;
            }

            default:
                break;
            }

            return expr.int_val;
        }

        int ss_interpreter::condition_passed(sis_expression *expr) {
            if (!expr || expr->type != sis_field_type::SISExpression) {
                return -1;
            }

            int pass = 0;

            if ((expr->left_expr && (expr->left_expr->op == ss_expr_op::EPrimTypeString)) || (expr->right_expr && (expr->right_expr->op == ss_expr_op::EPrimTypeString))) {
                if (expr->left_expr->op != expr->right_expr->op) {
                    LOG_ERROR(PACKAGE, "String expression can only be compared with string expression");
                    return -1;
                }

                switch (expr->op) {
                case ss_expr_op::EBinOpEqual: {
                    pass = (expr->left_expr->val.unicode_string == expr->right_expr->val.unicode_string);
                    break;
                }

                case ss_expr_op::EBinOpNotEqual: {
                    pass = (expr->left_expr->val.unicode_string != expr->right_expr->val.unicode_string);
                    break;
                }

                case ss_expr_op::EBinOpGreaterThan: {
                    pass = (expr->left_expr->val.unicode_string > expr->right_expr->val.unicode_string);
                    break;
                }

                case ss_expr_op::EBinOpLessThan: {
                    pass = (expr->left_expr->val.unicode_string < expr->right_expr->val.unicode_string);
                    break;
                }

                case ss_expr_op::EBinOpGreaterThanOrEqual: {
                    pass = (expr->left_expr->val.unicode_string >= expr->right_expr->val.unicode_string);
                    break;
                }

                case ss_expr_op::EBinOpLessOrEqual: {
                    pass = (expr->left_expr->val.unicode_string <= expr->right_expr->val.unicode_string);
                    break;
                }

                default: {
                    LOG_WARN(PACKAGE, "Unhandled string op type: {}", static_cast<int>(expr->op));
                    pass = -1;
                    break;
                }
                }

                return pass;
            }

            const int lhs = condition_passed(expr->left_expr.get());
            const int rhs = condition_passed(expr->right_expr.get());

            switch (expr->op) {
            case ss_expr_op::EBinOpEqual: {
                pass = (lhs == rhs);
                break;
            }

            case ss_expr_op::EBinOpNotEqual: {
                pass = (lhs != rhs);
                break;
            }

            case ss_expr_op::EBinOpGreaterThan: {
                pass = (lhs > rhs);
                break;
            }

            case ss_expr_op::EBinOpLessThan: {
                pass = (lhs < rhs);
                break;
            }

            case ss_expr_op::EBinOpGreaterThanOrEqual: {
                pass = (lhs >= rhs);
                break;
            }

            case ss_expr_op::EBinOpLessOrEqual: {
                pass = (lhs <= rhs);
                break;
            }

            case ss_expr_op::ELogOpAnd: {
                pass = static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs);
                break;
            }

            case ss_expr_op::ELogOpOr: {
                pass = static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs);
                break;
            }

            case ss_expr_op::EPrimTypeNumber:
            case ss_expr_op::EPrimTypeVariable: {
                pass = gasp_true_form_of_integral_expression(*expr);
                break;
            }

            case ss_expr_op::EUnaryOpNot: {
                pass = !lhs;
                break;
            }

            case ss_expr_op::EFuncExists: {
                pass = io->exist(expr->val.unicode_string);
                break;
            }

            default: {
                pass = -1;
                LOG_WARN(PACKAGE, "Unimplemented operation {} for expression", static_cast<int>(expr->op));
                break;
            }
            }

            return pass;
        }

        static bool fill_controller_registeration(io_system *io, loader::sis_controller *ctrl, package::object &parent, manager::controller_info &controller_info, const drive_number drive, const bool stub) {
            controller_info.data_ = reinterpret_cast<std::uint8_t *>(ctrl->raw_data.data());
            controller_info.size_ = ctrl->raw_data.size();

            parent.vendor_name = ctrl->info.vendor_name.unicode_string;
            parent.package_name = (reinterpret_cast<loader::sis_string *>(ctrl->info.names.fields[0].get()))->unicode_string;
            parent.uid = ctrl->info.uid.uid;
            parent.file_major_version = 5;
            parent.file_minor_version = 4;
            parent.in_rom = stub;
            parent.is_removable = !stub;
            parent.language = 1;
            parent.version.major = ctrl->info.version.major;
            parent.version.minor = ctrl->info.version.minor;
            parent.version.build = ctrl->info.version.build;

            parent.drives = stub ? (1 << (drive_z - drive_a)) : (1 << (drive - drive_a));
            parent.selected_drive = parent.drives;

            package::controller_info the_new_controller_info;
            the_new_controller_info.version.major = ctrl->info.version.major;
            the_new_controller_info.version.minor = ctrl->info.version.minor;
            the_new_controller_info.version.build = ctrl->info.version.build;

            parent.controller_infos.push_back(std::move(the_new_controller_info));

            LOG_INFO(PACKAGE, "Package {} registering with UID: 0x{:x}", common::ucs2_to_utf8(parent.package_name), parent.uid);

            for (std::size_t i = 0; i < ctrl->install_block.files.fields.size(); i++) {
                const loader::sis_file_des *file_des = reinterpret_cast<loader::sis_file_des *>(ctrl->install_block.files.fields[i].get());
                std::u16string file_path = file_des->target.unicode_string;

                if (file_path[0] == '!') {
                    file_path[0] = drive_to_char16(drive);
                } else {
                    parent.drives |= 1 << (char16_to_drive(file_path[0]) - drive_a);
                }

                // If we are really going to install this
                package::file_description desc;
                desc.index = file_des->idx;
                desc.mime_type = file_des->mime_type.unicode_string;
                desc.hash.algorithm = static_cast<std::uint32_t>(file_des->hash.hash_method);
                desc.hash.data.resize(file_des->hash.hash_data.raw_data.size());

                std::memcpy(desc.hash.data.data(), file_des->hash.hash_data.raw_data.data(), desc.hash.data.size());
                desc.operation = static_cast<std::int32_t>(file_des->op);
                desc.operation_options = static_cast<std::int32_t>(file_des->op_op);
                desc.uncompressed_length = file_des->len;
                desc.target = file_path;
                desc.sid = 0;

                if (!file_des->caps.raw_data.empty()) {
                    // It's an EXE file, so also gather the SID
                    symfile exe_file = io->open_file(desc.target, READ_MODE | BIN_MODE);
                    if (exe_file) {
                        eka2l1::ro_file_stream exe_stream(exe_file.get());

                        loader::e32img_header header;
                        loader::e32img_header_extended extended_header;
                        std::uint32_t uncomp_size = 0;
                        epocver ver_used = epocver::eka1;

                        extended_header.info.secure_id = 0;

                        if (loader::parse_e32img_header(reinterpret_cast<common::ro_stream *>(&exe_stream), header, extended_header,
                                uncomp_size, ver_used)
                            == 0) {
                            desc.sid = extended_header.info.secure_id;
                        }
                    }
                }

                parent.file_descriptions.push_back(std::move(desc));
            }

            for (auto &vendor_name_field : ctrl->info.vendor_names.fields) {
                loader::sis_string *vendor_name_string = reinterpret_cast<loader::sis_string *>(vendor_name_field.get());
                parent.localized_vendor_names.push_back(vendor_name_string->unicode_string);
            }

            for (auto &prop_field : ctrl->properties.properties.fields) {
                sis_property *property_real = reinterpret_cast<sis_property *>(prop_field.get());

                package::property new_prop;
                new_prop.key = property_real->key;
                new_prop.value = property_real->val;

                parent.properties.push_back(std::move(new_prop));
            }

            parent.signed_ = 1;
            parent.signed_by_sucert = 0;
            parent.trust_timestamp = common::get_current_utc_time_in_microseconds_since_0ad();
            parent.current_drives = parent.drives;
            parent.trust_status_value.revocation_status = package::revocation_ocsp_good;
            parent.trust_status_value.validation_status = package::validation_validated;
            parent.trust = package::package_trust_chain_validated_to_trust_anchor_and_ocsp_valid;
            parent.install_type = static_cast<package::install_type_value>(ctrl->info.install_type);

            return true;
        }

        bool ss_interpreter::interpret(sis_controller *controller, sis_registry_tree &me, const std::uint16_t base_data_idx) {
            // Set current controller
            current_controllers.push(controller);

            // Ask for language. If we can't choose the first one, or none
            if (controller->langs.langs.fields.size() != 1 && choose_lang) {
                std::vector<int> langs;

                for (auto &lang_field : controller->langs.langs.fields) {
                    langs.push_back(static_cast<int>(reinterpret_cast<sis_language *>(lang_field.get())->language));
                }

                controller->chosen_lang = static_cast<sis_lang>(choose_lang(&langs[0], static_cast<int>(langs.size())));

                if (controller->chosen_lang == static_cast<sis_lang>(-1)) {
                    std::string error_string = fmt::format("Abort choosing language, leading to installation canceled for controller 0x{:X}!",
                        controller->info.uid.uid);

                    LOG_ERROR(PACKAGE, "{}", error_string);

                    if (show_text) {
                        show_text(error_string.c_str(), true);
                    }

                    return false;
                }
            } else {
                controller->chosen_lang = reinterpret_cast<sis_language *>(controller->langs.langs.fields[0].get())->language;
            }

            if (!fill_controller_registeration(io, controller, me.package_info, me.controller_binary, install_drive, install_data->data_units.fields.empty())) {
                LOG_ERROR(PACKAGE, "Fail to fill package registeration");
            }

            const bool result = interpret(controller->install_block, me, base_data_idx + controller->idx.data_index);
            current_controllers.pop();

            return result;
        }

        bool ss_interpreter::interpret(sis_install_block &install_block, sis_registry_tree &parent_tree, std::uint16_t crr_blck_idx) {
            // Process file
            for (auto &wrap_file : install_block.files.fields) {
                sis_file_des *file = reinterpret_cast<sis_file_des *>(wrap_file.get());
                std::string raw_path = "";
                std::string install_path = "";

                if (file->target.unicode_string.length() > 0) {
                    install_path = get_install_path(file->target.unicode_string, install_drive);
                    raw_path = common::ucs2_to_utf8(*(io->get_raw_path(common::utf8_to_ucs2(install_path))));
                }

                switch (file->op) {
                case ss_op::text: {
                    auto buf = get_small_file_buf(file->idx, crr_blck_idx);

                    bool one_button = (static_cast<loader::ss_ft_option>(file->op_op) == ss_ft_option::let_continue);
                    bool result = true;

                    if ((buf.size() >= 2) && (buf[0] == 0xFF) && (buf[1] == 0xFE)) {
                        // BOM file
                        std::string converted_str = common::ucs2_to_utf8(std::u16string(reinterpret_cast<char16_t *>(buf.data()) + 1,
                            (buf.size() / 2) - 1));

                        if (show_text) {
                            result = show_text(reinterpret_cast<const char *>(converted_str.data()), one_button);
                        }
                    } else {
                        if (show_text) {
                            result = show_text(reinterpret_cast<const char *>(buf.data()), one_button);
                        }
                    }

                    LOG_INFO(PACKAGE, "EOpText: {}", reinterpret_cast<const char *>(buf.data()));

                    switch (static_cast<loader::ss_ft_option>(file->op_op)) {
                    case loader::ss_ft_option::skip_if_no: { // Skip next files
                        skip_next_file = !result;
                        break;
                    }

                    case loader::ss_ft_option::abort_if_no:
                    case loader::ss_ft_option::exit_if_no: {
                        if (!result) {
                            return false;
                        }

                        break;
                    }

                    default: {
                        break;
                    }
                    }

                    break;
                }

                case ss_op::undefined:
                case ss_op::install: {
                    if (!skip_next_file) {
                        if ((file->op == ss_op::undefined) && (!file->len || !file->uncompressed_len)) {
                            break;
                        }

                        bool lowered = false;

                        if (common::is_platform_case_sensitive()) {
                            raw_path = common::lowercase_string(raw_path);
                            lowered = true;
                        }

                        if (!install_data->data_units.fields.empty()) {
                            extract_target_info info;
                            info.file_path_ = raw_path;
                            info.data_unit_block_index_ = file->idx;
                            info.data_unit_index_ = crr_blck_idx;

                            extract_targets.push_back(info);
                            extract_target_accumulated_size += file->uncompressed_len;
                        }

                        if (!lowered) {
                            raw_path = common::lowercase_string(raw_path);
                        }

                        if (FOUND_STR(raw_path.find(".sis")) || FOUND_STR(raw_path.find(".sisx"))) {
                            if (!install_data->data_units.fields.empty() && (loader::identify_sis_type(raw_path).has_value())) {
                                LOG_INFO(PACKAGE, "Detected an SmartInstaller SIS, path at: {}", raw_path);
                                gathered_sis_paths.push_back(common::utf8_to_ucs2(raw_path));
                            }
                        }
                    } else {
                        skip_next_file = false;
                    }

                    break;
                }

                default:
                    break;
                }
            }

            for (auto &wrap_mini_pkg : install_block.controllers.fields) {
                sis_controller *ctrl = reinterpret_cast<sis_controller *>(wrap_mini_pkg.get());
                sis_registry_tree child_tree;

                if (interpret(ctrl, child_tree, crr_blck_idx)) {
                    parent_tree.embeds.push_back(std::move(child_tree));
                } else {
                    return false;
                }
            }

            // Parse if blocks
            for (auto &wrap_if_statement : install_block.if_blocks.fields) {
                sis_if *if_stmt = reinterpret_cast<sis_if *>(wrap_if_statement.get());
                auto result = condition_passed(&if_stmt->expr);

                if (result) {
                    interpret(if_stmt->install_block, parent_tree, crr_blck_idx);
                } else {
                    for (auto &wrap_else_branch : if_stmt->else_if.fields) {
                        sis_else_if *else_branch = reinterpret_cast<sis_else_if *>(wrap_else_branch.get());

                        if (condition_passed(&else_branch->expr)) {
                            interpret(else_branch->install_block, parent_tree, crr_blck_idx);
                            break;
                        }
                    }
                }
            }

            return true;
        }

        static void fill_embeds_to_package_info(sis_registry_tree &self) {
            for (auto &embed : self.embeds) {
                package::package embed_info;
                embed_info.index = static_cast<std::int32_t>(self.package_info.embedded_packages.size());
                embed_info.package_name = embed.package_info.package_name;
                embed_info.vendor_name = embed.package_info.vendor_name;
                embed_info.uid = embed.package_info.uid;

                self.package_info.embedded_packages.push_back(std::move(embed_info));

                fill_embeds_to_package_info(embed);
                self.package_info.embedded_packages.insert(self.package_info.embedded_packages.end(), embed.package_info.embedded_packages.begin(), embed.package_info.embedded_packages.end());
            }
        }

        std::unique_ptr<sis_registry_tree> ss_interpreter::interpret(progress_changed_callback cb, cancel_requested_callback ccb) {
            if (install_data->data_units.fields.empty()) {
                LOG_INFO(PACKAGE, "Interpreting a stub SIS");
            }

            std::unique_ptr<sis_registry_tree> trees = std::make_unique<sis_registry_tree>();

            gathered_sis_paths.clear();
            extract_targets.clear();

            extract_target_accumulated_size = 0;
            extract_target_decomped_size = 0;

            progress_changed_cb = cb;
            cancel_cb = ccb;

            if (!interpret(main_controller, *trees, 0)) {
                return nullptr;
            }

            if (extract_targets.empty()) {
                if (cb)
                    cb(1, 1);
            } else {
                std::size_t n = 0;
                bool cancel_requested = false;

                for (; n < extract_targets.size(); n++) {
                    extract_target_info &target = extract_targets[n];
                    if (!extract_file(target.file_path_, target.data_unit_block_index_, target.data_unit_index_)) {
                        cancel_requested = true;
                        break;
                    }
                }

                if (cancel_requested) {
                    for (std::size_t i = 0; i < n; i++) {
                        common::remove(extract_targets[i].file_path_);
                    }

                    return nullptr;
                }
            }

            fill_embeds_to_package_info(*trees);
            return trees;
        }
    }
}
