/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <epoc/services/msv/common.h>
#include <epoc/services/msv/registry.h>
#include <epoc/loader/rsc.h>
#include <epoc/vfs.h>

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <epoc/utils/des.h>

namespace eka2l1::epoc::msv {
    static const std::u16string DEFAULT_MSG_REG_LIST_FILE = u"C:\\private\\1000484b\\Mtm Registry v2";

    mtm_registry::mtm_registry(io_system *io)
        : io_(io) { 
        list_path_ = DEFAULT_MSG_REG_LIST_FILE;
    }

    bool mtm_registry::install_group_from_rsc(const std::u16string &path) {
        symfile rsc_file = io_->open_file(path, READ_MODE | BIN_MODE);

        if (!rsc_file) {
            return false;
        }

        eka2l1::ro_file_stream rsc_file_stream(rsc_file.get());
        loader::rsc_file rsc_file_loader(reinterpret_cast<common::ro_stream*>(&rsc_file_stream));

        std::vector<std::uint8_t> info = rsc_file_loader.read(1);        // Info
        common::chunkyseri info_reader(info.data(), info.size(), common::SERI_MODE_READ);

        mtm_group new_group;
        new_group.ref_count_ = 0;

        info_reader.absorb(new_group.mtm_uid_);
        info_reader.absorb(new_group.tech_type_uid_);

        std::uint16_t total_comps = 0;
        info_reader.absorb(total_comps);

        mtm_component *starter = &new_group.comps_;

        for (std::uint16_t i = 0; i < total_comps; i++) {
            starter->next_ = new mtm_component;
            mtm_component &comp = *starter->next_;

            starter = starter->next_;

            comp.group_idx_ = static_cast<std::uint32_t>(groups_.size());

            loader::absorb_resource_string(info_reader, comp.name_);
            info_reader.absorb(comp.comp_uid_);
            info_reader.absorb(comp.specific_uid_);
            info_reader.absorb(comp.entry_point_);
            info_reader.absorb(comp.major_);
            info_reader.absorb(comp.minor_);
            info_reader.absorb(comp.build_);

            if (comp.specific_uid_ == MTM_DEFAULT_SPECIFIC_UID) {
                loader::absorb_resource_string(info_reader, comp.filename_);
            }

            const std::uint32_t comp_uid = comp.comp_uid_;

            comps_[comp_uid].push_back(&comp);
        }
        
        // Check out capability
        info = rsc_file_loader.read(2);
        
        if (!info.empty()) {
            info_reader = common::chunkyseri(info.data(), info.size(), common::SERI_MODE_READ);
            new_group.cap_avail_ = 1;
            
            info_reader.absorb(new_group.cap_send_);
            info_reader.absorb(new_group.cap_body_);
        }

        groups_.push_back(std::move(new_group));
        return true;
    }

    bool mtm_registry::install_group(const std::u16string &path) {
        const std::u16string path_lower = common::lowercase_ucs2_string(path);
        
        // Try to find if this group is already loaded
        if (std::find(mtm_files_.begin(), mtm_files_.end(), path_lower) != mtm_files_.end()) {
            return false;
        }

        const std::u16string ext = eka2l1::path_extension(path);

        if ((ext.length() >= 2) && ((ext[1] == u'r') || (ext[1] == u'R'))) {
            if (install_group_from_rsc(path)) {        
                // Install this path
                add_entry_to_mtm_list(path_lower);
                return true;
            } else {
                LOG_ERROR("Unable to install MTM group due to file being corrupted {}", common::ucs2_to_utf8(path));
                return false;
            }
        }

        LOG_ERROR("Unable to install MTM group due to unrecognised file {}", common::ucs2_to_utf8(path));
        return false;
    }

    mtm_group* mtm_registry::query_mtm_group(const epoc::uid the_uid) {
        for (std::size_t i = 0; i < groups_.size(); i++) {
            if (groups_[i].mtm_uid_ == the_uid) {
                return &groups_[i];
            }
        }

        return nullptr;
    }

    std::vector<mtm_component*> &mtm_registry::get_components(const epoc::uid the_uid) {
        return comps_[the_uid];
    }
    
    void mtm_registry::load_mtm_list() {
        symfile list_file = io_->open_file(list_path_, READ_MODE | BIN_MODE);

        if (!list_file) {
            LOG_TRACE("MTM registry file not yet present");
            return;
        }

        eka2l1::ro_file_stream list_file_stream(list_file.get());
        
        while (list_file_stream.valid()) {
            std::string mtm_path;
            mtm_path = list_file_stream.read_string();

            install_group(common::utf8_to_ucs2(mtm_path));
        }
    }

    void mtm_registry::save_mtm_list() {
        symfile list_file = io_->open_file(list_path_, WRITE_MODE | BIN_MODE);

        if (!list_file) {
            LOG_TRACE("MTM registry file can not be open for write");
            return;
        }

        eka2l1::wo_file_stream list_file_stream(list_file.get());

        for (auto &path: mtm_files_) {
            list_file_stream.write_string(common::ucs2_to_utf8(path));
        }
    }

    void mtm_registry::add_entry_to_mtm_list(const std::u16string &path) {
        mtm_files_.push_back(path);
        save_mtm_list();
    }
}