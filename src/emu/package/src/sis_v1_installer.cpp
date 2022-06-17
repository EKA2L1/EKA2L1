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

#include <loader/sis.h>
#include <loader/sis_old.h>

#include <package/sis_v1_installer.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/flate.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>

#include <cwctype>
#include <vfs/vfs.h>

namespace eka2l1::loader {
    std::uint32_t sis_old_get_attribute_value(const std::uint32_t attrib_val, var_value_resolver_func resolver_cb, const language choosen_lang) {
        // Same as new SIS version
        switch (attrib_val) {
        case 0x1000:
            return static_cast<std::uint32_t>(choosen_lang);
        
        default:
            break;
        }

        if (resolver_cb) {
            return resolver_cb(attrib_val);
        }

        return 0;
    }

    std::uint32_t sis_old_evaluate_numeric_expression(sis_old_expression &expression, io_system *io, var_value_resolver_func resolver_cb,
        const language choosen_lang) {
        switch (expression.expression_type)  {
        case sis_old_file_expression_type_number:
            return static_cast<sis_old_number_expression&>(expression).value;

        case sis_old_file_expression_type_attribute:
            return sis_old_get_attribute_value(static_cast<sis_old_attribute_expression&>(expression).attribute,
                resolver_cb, choosen_lang);

        case sis_old_file_expression_type_exists: {
            sis_old_unary_expression &exist_expr = static_cast<sis_old_unary_expression&>(expression);
            if (!exist_expr.target_expression || (exist_expr.target_expression->expression_type != sis_old_file_expression_type_string)) {
                LOG_ERROR(PACKAGE, "Invalid argument to function FILEEXIST(path). Expect path to be string, but expression type is {}",
                    (exist_expr.target_expression ? "not even exist" : common::to_string(exist_expr.target_expression->expression_type)));
                break;
            }

            return io->exist(static_cast<sis_old_string_expression&>(*exist_expr.target_expression).value);
        }

        case sis_old_file_expression_type_devcap:
        case sis_old_file_expression_type_appcap:
            LOG_TRACE(PACKAGE, "DEVCAP and APPCAP expression are not supported yet!");
            break;

        case sis_old_file_expression_type_not: {
            sis_old_unary_expression &not_expr = static_cast<sis_old_unary_expression&>(expression);
            if (!not_expr.target_expression) {
                LOG_ERROR(PACKAGE, "No expression passed to logic operator NOT!");
                break;
            }
            return !sis_old_evaluate_numeric_expression(*not_expr.target_expression, io, resolver_cb, choosen_lang);
        }

        case sis_old_file_expression_type_equal:
        case sis_old_file_expression_type_lequal:
        case sis_old_file_expression_type_and:
        case sis_old_file_expression_type_gequal:
        case sis_old_file_expression_type_greater:
        case sis_old_file_expression_type_less:
        case sis_old_file_expression_type_not_equal:
        case sis_old_file_expression_type_or: {
            sis_old_binary_expression &bin_expr = static_cast<sis_old_binary_expression&>(expression);
            std::uint32_t lhs = 0;
            std::uint32_t rhs = 0;

            if (bin_expr.lhs)
                lhs = sis_old_evaluate_numeric_expression(*bin_expr.lhs, io, resolver_cb, choosen_lang);
                
            if (bin_expr.rhs)
                rhs = sis_old_evaluate_numeric_expression(*bin_expr.rhs, io, resolver_cb, choosen_lang);

            switch (expression.expression_type) {
            case sis_old_file_expression_type_equal:
                return (lhs == rhs);

            case sis_old_file_expression_type_not_equal:
                return (lhs != rhs);

            case sis_old_file_expression_type_less:
                return (lhs < rhs);

            case sis_old_file_expression_type_lequal:
                return (lhs <= rhs);

            case sis_old_file_expression_type_greater:
                return (lhs > rhs);

            case sis_old_file_expression_type_gequal:
                return (lhs >= rhs);

            case sis_old_file_expression_type_and:
                return (lhs & rhs);

            case sis_old_file_expression_type_or:
                return (lhs | rhs);

            default:
                break;
            }

            break;
        }

        default:
            LOG_WARN(PACKAGE, "Unknown SIS expression type {}!", expression.expression_type);
            break;
        }

        return 0;
    }

    bool sis_old_is_condition_passed(sis_old_expression *expression, io_system *io, var_value_resolver_func resolver_cb,
        const language choosen_lang) {
        if (!expression) {
            return true;
        }

        return (sis_old_evaluate_numeric_expression(*expression, io, resolver_cb, choosen_lang) != 0);
    }
    
    void sis_old_evaluate_block(sis_old_block &block, std::vector<sis_old_file*> &files, io_system *io, var_value_resolver_func resolver_cb,
        const language choosen_lang) {
        std::vector<std::unique_ptr<sis_old_file_record>> *record_to_iterate = &block.false_commands;

        if (sis_old_is_condition_passed(block.condition.get(), io, resolver_cb, choosen_lang)) {
            record_to_iterate = &block.true_commands;
        }

        for (std::size_t i = 0; i < record_to_iterate->size(); i++) {
            switch (record_to_iterate->at(i)->file_record_type) {
            case file_record_type_block:
                sis_old_evaluate_block(*reinterpret_cast<sis_old_block*>(record_to_iterate->at(i).get()), files, io, resolver_cb, choosen_lang);
                break;

            case file_record_type_simple_file:
            case file_record_type_multiple_lang_file:
                files.push_back(reinterpret_cast<sis_old_file*>(record_to_iterate->at(i).get()));
                break;

            default:
                break;
            }
        }
    }

    bool install_sis_old(const std::u16string &path, io_system *io, drive_number drive,
        package::object &info, choose_lang_func choose_lang_cb, var_value_resolver_func resolver_cb,
        progress_changed_callback progress_cb, cancel_requested_callback cancel_cb) {
        std::optional<sis_old> res = *loader::parse_sis_old(common::ucs2_to_utf8(path));
        if (!res.has_value()) {
            return false;
        }

        std::vector<std::u16string> more_sis;

        info.drives = 1 << (drive - drive_a);
        info.uid = res->header.uid1;
        info.package_name = res->comp_names[0];
        info.vendor_name = u"Unknown";
        info.install_type = package::install_type_normal_install;

        info.supported_language_ids.resize(res->langs.size());
        for (std::size_t i = 0; i < info.supported_language_ids.size(); i++) {
            info.supported_language_ids[i] = static_cast<std::int32_t>(res->langs[i]);
        }

        info.current_drives = info.drives;
        info.is_removable = true;

        language choosen_language = language::en;
        std::size_t choosen_language_index = 0;

        if (res->langs.size() >= 1) {
            choosen_language = res->langs[0];
        }

        if ((res->langs.size() > 1) && choose_lang_cb) {
            choosen_language = static_cast<language>(choose_lang_cb(reinterpret_cast<const int*>(res->langs.data()),
                static_cast<int>(res->langs.size())));

            for (std::size_t i = 0; i < res->langs.size(); i++) {
                if (res->langs[i] == choosen_language) {
                    choosen_language_index = i;
                    break;
                }
            }
        }

        std::vector<sis_old_file*> files_note;
        sis_old_evaluate_block(res->root_block, files_note, io, resolver_cb, choosen_language);

        std::size_t total_size = 0;
        std::size_t decomped = 0;

        for (auto &file : files_note) {
            if ((choosen_language_index < file->file_infos.size()) && (file->file_record_type == file_record_type_multiple_lang_file)) {
                total_size += file->file_infos[choosen_language_index].original_length;
            } else {
                total_size += file->file_infos[0].original_length;
            }
        }

        if (files_note.empty()) {
            if (progress_cb)
                progress_cb(1, 1);
        }

        std::size_t processed = 0;
        bool canceled = false;

        for (; processed < files_note.size(); processed++) {
            loader::sis_old_file *file = files_note[processed];
            if (cancel_cb && cancel_cb()) {
                canceled = true;
                break;
            }

            if ((file->file_type != 0) && (file->file_type != 2)) {
                continue;
            }

            std::u16string dest = file->dest;
            if (file->file_type == 2) {
                static const char16_t *TEMP_SIS_FOLDER_PATH = u"E:\\system\\install\\temp\\";
                dest = std::u16string(TEMP_SIS_FOLDER_PATH) + eka2l1::filename(file->name);
                io->create_directories(TEMP_SIS_FOLDER_PATH);
            }

            if (dest.find(u"!") != std::u16string::npos) {
                dest[0] = drive_to_char16(drive);
            }

            if ((file->file_type != 1) && dest != u"") {
                std::string rp = eka2l1::file_directory(common::ucs2_to_utf8(dest));
                io->create_directories(common::utf8_to_ucs2(rp));
            } else {
                continue;
            }

            package::file_description desc;
            desc.operation = static_cast<std::int32_t>(loader::ss_op::install);
            desc.target = dest;

            symfile f = io->open_file(dest, WRITE_MODE | BIN_MODE);

            LOG_TRACE(PACKAGE, "Installing file {}", common::ucs2_to_utf8(dest));

            loader::sis_old_file::individual_file_data_info *data_info = nullptr;
            
            if ((choosen_language_index < file->file_infos.size()) && (file->file_record_type == file_record_type_multiple_lang_file)) {
                data_info = &file->file_infos[choosen_language_index];
            } else {
                data_info = &file->file_infos[0];
            }

            size_t left = data_info->length;
            size_t chunk = 0x2000;

            std::vector<char> temp;
            temp.resize(chunk);

            std::vector<char> inflated;
            inflated.resize(0x100000);

            common::ro_std_file_stream sis_file(common::ucs2_to_utf8(path), true);
            sis_file.seek(data_info->position, common::seek_where::beg);

            mz_stream stream;

            stream.zalloc = nullptr;
            stream.zfree = nullptr;

            if (!(res->header.op & 0x8)) {
                if (inflateInit(&stream) != MZ_OK) {
                    return false;
                }
            }

            desc.uncompressed_length = left;
            desc.operation_options = 0;
            desc.index = static_cast<std::uint32_t>(info.file_descriptions.size());
            desc.sid = 0;

            while (left > 0) {
                if (cancel_cb && cancel_cb()) {
                    canceled = true;
                    break;
                }

                size_t took = left < chunk ? left : chunk;
                size_t readed = sis_file.read(temp.data(), took);

                if (readed != took) {
                    if (f->tell() != f->size()) {
                        // End of file not reached
                        canceled = true;
                    }

                    f->close();
                    break;
                }

                if (res->header.op & 0x8) {
                    f->write_file(temp.data(), 1, static_cast<uint32_t>(took));
                    decomped += took;
                } else {
                    uint32_t inf;
                    flate::inflate_data(&stream, temp.data(), inflated.data(), static_cast<uint32_t>(took), &inf);

                    f->write_file(inflated.data(), 1, inf);
                    decomped += inf;
                }

                if (progress_cb) {
                    if (total_size != 0) {
                        progress_cb(decomped, total_size);
                    } else {
                        progress_cb(1, 1);
                    }
                }

                left -= took;
            }

            if (!(res->header.op & 0x8))
                inflateEnd(&stream);

            std::optional<std::u16string> raw_path_dest = io->get_raw_path(dest);
            if (raw_path_dest.has_value()) {
                if (canceled) {
                    common::remove(common::ucs2_to_utf8(raw_path_dest.value()));
                } else {
                    if (file->file_type == 2) {
                        more_sis.push_back(raw_path_dest.value());
                    }
                }
            }

            if (canceled) {
                break;
            }

            info.file_descriptions.push_back(std::move(desc));
        }

        if (canceled) {
            for (std::size_t i = 0; i < processed; i++) {
                common::remove(common::ucs2_to_utf8(info.file_descriptions[i].target));
            }

            return false;
        }

        for (const std::u16string &path_more_sis: more_sis) {
            if (!install_sis_old(path_more_sis, io, drive, info, choose_lang_cb, resolver_cb, progress_cb, cancel_cb)) {
                return false;
            }

            common::remove(common::ucs2_to_utf8(path_more_sis));
        }

        return true;
    }
}
