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

#include <services/etel/line.h>
#include <services/etel/modmngr.h>
#include <services/etel/phone.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <vfs/vfs.h>

namespace eka2l1::epoc::etel {
    module_manager::module_manager() {
    }

    std::string module_manager::get_full_tsy_path(io_system *io, const std::string &module_name) {
        std::string fulled_module_path = module_name;

        if (eka2l1::path_extension(fulled_module_path).empty()) {
            fulled_module_path += ".tsy";
        }

        if (!eka2l1::has_root_name(fulled_module_path, true)) {
            for (drive_number drv = drive_z; drv >= drive_a; drv--) {
                if (io->get_drive_entry(drv)) {
                    std::string absoluted = eka2l1::absolute_path(fulled_module_path, fmt::format(
                        "{}:\\system\\libs\\", static_cast<char>(drive_to_char16(drv))));

                    if (io->exist(common::utf8_to_ucs2(absoluted))) {
                        fulled_module_path = absoluted;
                        break;
                    }
                }
            }
        }

        return fulled_module_path;
    }

    bool module_manager::load_tsy(io_system *io, const kernel::uid borrowed_session, const std::string &module_name) {
        const std::string module_lowercased = common::lowercase_string(get_full_tsy_path(io, module_name));
        auto find_result = std::find_if(loaded_.begin(), loaded_.end(), [module_lowercased](const tsy_module_info &info) {
            return info.name_ == module_lowercased;
        });
    
        if (find_result != loaded_.end()) {
            // The module already loaded.
            if (!std::binary_search(find_result->used_sessions_.begin(), find_result->used_sessions_.end(), borrowed_session)) {
                find_result->used_sessions_.push_back(borrowed_session);
                std::sort(find_result->used_sessions_.begin(), find_result->used_sessions_.end());
            }

            return true;
        }

        LOG_TRACE(SERVICE_ETEL, "Loading TSY temporary stubbed with module name {}", module_name);

        // TODO: We need to give it a proper info
        // Give a line first, add it to phone later
        etel_module_entry line_entry;
        line_entry.tsy_name_ = module_lowercased;

        epoc::etel_line_info line_info;
        line_info.hook_sts_ = epoc::etel_line_hook_sts_off;
        line_info.sts_ = epoc::etel_line_status_idle;
        line_info.last_call_added_.set_length(nullptr, 0);
        line_info.last_call_answering_.set_length(nullptr, 0);

        line_entry.entity_ = std::make_unique<etel_line>(line_info, module_name + "::Line1",
            epoc::etel_line_caps_voice);

        etel_module_entry phone_entry;
        phone_entry.tsy_name_ = module_lowercased;

        epoc::etel_phone_info phone_info;
        phone_info.exts_ = 0;
        phone_info.network_ = epoc::etel_network_type_mobile_digital;

        const std::u16string phone_name_meme = common::utf8_to_ucs2(module_name) + u"::Phone";

        phone_info.phone_name_.assign(nullptr, phone_name_meme);

        auto phone_obj = std::make_unique<etel_phone>(phone_info);
        phone_obj->lines_.push_back(reinterpret_cast<etel_line *>(line_entry.entity_.get()));

        phone_entry.entity_ = std::move(phone_obj);

        entries_.push_back(std::move(phone_entry));
        entries_.push_back(std::move(line_entry));

        tsy_module_info info;
        info.name_ = module_lowercased;
        info.used_sessions_.push_back(borrowed_session);

        // Find a free slot actually
        find_result = std::find_if(loaded_.begin(), loaded_.end(), [module_lowercased](const tsy_module_info &info) {
            return info.used_sessions_.empty();
        });

        if (find_result != loaded_.end()) {
            *find_result = info;
        } else {
            loaded_.push_back(info);
        }

        return true;
    }

    bool module_manager::close_tsy(io_system *io, const kernel::uid borrowed, const std::string &module_name) {
        const std::string module_lowercased = common::lowercase_string(get_full_tsy_path(io, module_name));

        auto name_res = std::find_if(loaded_.begin(), loaded_.end(), [module_lowercased](const tsy_module_info &info) {
            return info.name_ == module_lowercased;
        });

        if (name_res == loaded_.end()) {
            return false;
        }

        auto borrow_res = std::lower_bound(name_res->used_sessions_.begin(), name_res->used_sessions_.end(), borrowed);

        if ((borrow_res == name_res->used_sessions_.end()) || (*borrow_res != borrowed)) {
            return false;
        }

        // The order should stay the same when we remove one integer
        name_res->used_sessions_.erase(borrow_res);

        if (name_res->used_sessions_.empty()) {
            auto entry_res = std::remove_if(entries_.begin(), entries_.end(),
                [=](const auto &entry) {
                    if (common::lowercase_string(entry.tsy_name_) == module_lowercased) {
                        return true;
                    }
                    return false;
                });

            entries_.erase(entry_res, entries_.end());
        }

        return true;
    }

    void module_manager::unload_from_sessions(io_system *io, const kernel::uid borrowed_session) {
        for (std::size_t i = 0; i < loaded_.size(); i++) {
            if (std::binary_search(loaded_[i].used_sessions_.begin(), loaded_[i].used_sessions_.end(), borrowed_session)) {
                close_tsy(io, borrowed_session, loaded_[i].name_);
            }
        }
    }

    std::optional<std::uint32_t> module_manager::get_entry_real_index(const std::uint32_t respective_index, const etel_entry_type type) {
        std::int32_t i = -1;

        while ((i != respective_index) && ((i < 0) || ((i < entries_.size()) && entries_[i].entity_->type() != type)))
            i++;

        if (i == entries_.size() || (i < 0)) {
            return std::nullopt;
        }

        return i;
    }

    bool module_manager::get_entry(const std::uint32_t real_index, etel_module_entry **entry) {
        if (real_index >= entries_.size()) {
            return false;
        }

        *entry = &entries_[real_index];
        return true;
    }

    bool module_manager::get_entry_by_name(const std::string &name, etel_module_entry **entry) {
        auto result = std::find_if(entries_.begin(), entries_.end(), [name](etel_module_entry &search_entry) {
            if (search_entry.entity_->type() == etel_entry_phone) {
                etel_phone &phone = static_cast<etel_phone &>(*search_entry.entity_);
                return common::ucs2_to_utf8(phone.info_.phone_name_.to_std_string(nullptr)) == name;
            }

            return false;
        });

        if (result == entries_.end()) {
            return false;
        }

        *entry = &(*result);
        return true;
    }

    std::size_t module_manager::total_entries(const etel_entry_type type) const {
        std::size_t total = 0;

        for (std::size_t i = 0; i < entries_.size(); i++) {
            if (entries_[i].entity_->type() == type) {
                total++;
            }
        }

        return total;
    }
}