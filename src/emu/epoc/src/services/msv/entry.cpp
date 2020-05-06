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

#include <common/chunkyseri.h>
#include <common/log.h>
#include <common/path.h>
#include <common/time.h>

#include <epoc/services/msv/common.h>
#include <epoc/services/msv/entry.h>
#include <epoc/vfs.h>
#include <epoc/loader/rsc.h>

#include <epoc/utils/bafl.h>
#include <epoc/utils/err.h>

namespace eka2l1::epoc::msv {
    entry_indexer::entry_indexer(io_system *io, const std::u16string &msg_folder, const language preferred_lang)
        : io_(io)
        , rom_drv_(drive_z)
        , preferred_lang_(preferred_lang)
        , msg_dir_(msg_folder) {
        drive_number drv_target = drive_z;

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            auto drv_entry = io->get_drive_entry(drv);

            if (drv_entry && (drv_entry->media_type == drive_media::rom)) {
                rom_drv_ = drv;
                break;
            }
        }

        if (!load_entries_file(drive_c)) {
            entries_.clear();

            create_root_entry();
            create_standard_entries(drive_c);
        }
    }

    static bool msv_entry_comparisor(const entry &lhs, const entry &rhs) {
        return (lhs.id_ & 0x0FFFFFFF) < (rhs.id_ & 0x0FFFFFFF);
    }

    struct entry_index_info {
        std::uint32_t id_;
        std::uint32_t service_id_;
        std::uint32_t parent_id_;
        std::uint32_t flags_;
    };

    bool entry_indexer::load_entries_file(drive_number crr_drive) {
        std::u16string msg_entries_file = eka2l1::add_path(msg_dir_, u"Entries.chs");
    
        // Open the entry file first
        symfile entry_file = io_->open_file(msg_entries_file, READ_MODE | BIN_MODE);

        if (!entry_file) {
            LOG_ERROR("Unable to open entries file to load entries list!");
            return false;
        }

        entry_index_info index_info;

        while (entry_file->valid()) {
            if (entry_file->read_file(&index_info, sizeof(entry_index_info), 1) != sizeof(entry_index_info)) {
                break;
            }

            epoc::msv::entry ent;
            ent.id_ = index_info.id_;
            ent.parent_id_ = index_info.parent_id_;
            ent.service_id_ = index_info.service_id_;
            ent.flags_ = index_info.flags_;

            entries_.push_back(ent);
        }

        // Now that we complete the tree, load all the data
        for (std::size_t i = 0; i < entries_.size(); i++) {
            if (!load_entry_data(entries_[i])) {
                LOG_WARN("Unable to load entry data for entry ID {}", entries_[i].id_);
            }
        }

        return true;
    }

    bool entry_indexer::update_entries_file(const std::size_t entry_pos_start) {
        std::u16string msg_entries_file = eka2l1::add_path(msg_dir_, u"Entries.chs");
    
        // Open the entry file first
        symfile entry_file = io_->open_file(msg_entries_file, WRITE_MODE | BIN_MODE);

        if (!entry_file) {
            LOG_ERROR("Unable to open entries file to update entries list!");
            return false;
        }

        entry_file->seek(entry_pos_start * sizeof(entry_index_info), file_seek_mode::beg);

        for (std::size_t i = entry_pos_start; i < entries_.size(); i++) {
            entry_index_info index_info;

            index_info.id_ = entries_[i].id_;
            index_info.parent_id_ = entries_[i].parent_id_;
            index_info.service_id_ = entries_[i].service_id_;
            index_info.flags_ = entries_[i].flags_;

            if (entry_file->write_file(&index_info, sizeof(entry_index_info), 1) != sizeof(entry_index_info)) {
                LOG_ERROR("Unable to update MSV entry number {} (write failure)", i);
                return false;
            }
        }

        return true;
    }

    enum msv_folder_type {
        MSV_FOLDER_TYPE_NORMAL = 0,
        MSV_FOLDER_TYPE_PATH = 1 << 0,
        MSV_FOLDER_TYPE_SERVICE = 1 << 1
    };

    static std::u16string get_folder_name(const std::uint32_t id, const msv_folder_type type) {
        if ((id == 0) && (type == MSV_FOLDER_TYPE_SERVICE)) {
            return u"";
        }

        std::u16string in_hex = fmt::format(u"{:08X}", id);

        switch (type) {
        case MSV_FOLDER_TYPE_PATH:
            in_hex += u"_F";
            break;

        case MSV_FOLDER_TYPE_SERVICE:
            in_hex += u"_S";
            break;

        default:
            // Intentional
            break;
        }

        return in_hex;
    }

    std::optional<std::u16string> entry_indexer::get_entry_data_file(entry &ent) {
        std::u16string file_path = eka2l1::add_path(msg_dir_, get_folder_name(ent.service_id_, MSV_FOLDER_TYPE_SERVICE));
        entry &back_trace = ent;

        std::u16string parent_dir;

        while (back_trace.parent_id_ != epoc::error_not_found) {
            parent_dir = get_folder_name(ent.parent_id_, MSV_FOLDER_TYPE_PATH) + u"\\" + parent_dir;
            
            entry my_parent;
            my_parent.id_ = back_trace.parent_id_;

            auto ent_ite = std::lower_bound(entries_.begin(), entries_.end(), my_parent, msv_entry_comparisor);
            
            if ((ent_ite == entries_.end()) || (ent_ite->id_ != back_trace.parent_id_)) {
                break;
            }
            
            back_trace = *ent_ite;
        }

        file_path = eka2l1::add_path(file_path, parent_dir);
        
        if (!io_->exist(file_path)) {
            io_->create_directories(file_path);
        }
        
        return eka2l1::add_path(file_path, get_folder_name(ent.id_, MSV_FOLDER_TYPE_NORMAL));
    }

    bool entry_indexer::save_entry_data(entry &ent) {
        // Access the service folder first
        std::optional<std::u16string> file_path = get_entry_data_file(ent);

        if (!file_path) {
            LOG_ERROR("Unable to construct entry data path for entry ID {}", ent.id_);
            return false;
        }

        symfile entry_file = io_->open_file(file_path.value(), WRITE_MODE | BIN_MODE);

        if (!entry_file) {
            LOG_ERROR("Unable to open entry data file!");
            return false;
        }

        wo_file_stream entry_file_stream(entry_file.get());

        entry_file_stream.write(&ent.id_, sizeof(std::uint32_t));
        entry_file_stream.write(&ent.parent_id_, sizeof(std::int32_t));
        entry_file_stream.write(&ent.service_id_, sizeof(std::uint32_t));
        entry_file_stream.write(&ent.type_uid_, sizeof(epoc::uid));
        entry_file_stream.write(&ent.mtm_uid_, sizeof(epoc::uid));
        entry_file_stream.write(&ent.data_, sizeof(std::uint32_t));
        
        entry_file_stream.write_string(ent.description_);
        entry_file_stream.write_string(ent.details_);

        entry_file_stream.write(&ent.time_, sizeof(std::uint64_t));
        entry_file_stream.write(&ent.flags_, sizeof(std::uint32_t));

        return true;
    }

    bool entry_indexer::load_entry_data(entry &ent) {
        // Access the service folder first
        std::optional<std::u16string> file_path = get_entry_data_file(ent);

        if (!file_path) {
            LOG_ERROR("Unable to construct entry data path for entry ID {}", ent.id_);
            return false;
        }

        symfile entry_file = io_->open_file(file_path.value(), READ_MODE | BIN_MODE);

        if (!entry_file) {
            LOG_ERROR("Unable to open entry data file!");
            return false;
        }

        ro_file_stream entry_file_stream(entry_file.get());

        entry_file_stream.read(&ent.id_, sizeof(std::uint32_t));
        entry_file_stream.read(&ent.parent_id_, sizeof(std::int32_t));
        entry_file_stream.read(&ent.service_id_, sizeof(std::uint32_t));
        entry_file_stream.read(&ent.type_uid_, sizeof(epoc::uid));
        entry_file_stream.read(&ent.mtm_uid_, sizeof(epoc::uid));
        entry_file_stream.read(&ent.data_, sizeof(std::uint32_t));
        
        ent.description_ = entry_file_stream.read_string<char16_t>();
        ent.details_ = entry_file_stream.read_string<char16_t>();

        entry_file_stream.read(&ent.time_, sizeof(std::uint64_t));
        entry_file_stream.read(&ent.flags_, sizeof(std::uint32_t));

        return true;
    }

    bool entry_indexer::add_entry(entry &ent) {
        // Find an entry with existing ID, if it exists then this entry will not be added.
        if (std::binary_search(entries_.begin(), entries_.end(), ent, msv_entry_comparisor)) {
            return false;
        }

        // Add this entry. And sort by ID
        entries_.push_back(ent);
        std::sort(entries_.begin(), entries_.end(), msv_entry_comparisor);

        // Find the entry again in this sort list, and update the entries file from this position.
        auto ite_pos = std::lower_bound(entries_.begin(), entries_.end(), ent, msv_entry_comparisor);
        if (!update_entries_file(std::distance(entries_.begin(), ite_pos))) {
            return false;
        }

        // Save this entry to the correspond folder
        return save_entry_data(*ite_pos);
    }

    bool entry_indexer::create_root_entry() {
        epoc::msv::entry root_entry;
        root_entry.id_ = 0x1000;
        root_entry.mtm_uid_ = MTM_SERVICE_UID_ROOT;
        root_entry.type_uid_ = MTM_SERVICE_UID_ROOT;
        root_entry.parent_id_ = epoc::error_not_found;
        root_entry.data_ = 0;
        root_entry.time_ = common::get_current_time_in_microseconds_since_epoch();

        return add_entry(root_entry);
    }

    bool entry_indexer::create_standard_entries(drive_number crr_drive) {
        std::u16string DEFAULT_STANDARD_ENTRIES_FILE = u"!:\\resource\\messaging\\msgs.rsc";
        DEFAULT_STANDARD_ENTRIES_FILE[0] = drive_to_char16(rom_drv_);

        const std::u16string nearest_default_entries_file = utils::get_nearest_lang_file(io_,
            DEFAULT_STANDARD_ENTRIES_FILE, preferred_lang_, rom_drv_);

        // Open resource loader and read this file
        symfile nearest_default_entries_file_io = io_->open_file(nearest_default_entries_file, READ_MODE | BIN_MODE);

        if (!nearest_default_entries_file_io) {
            LOG_ERROR("Unable to create standard entries (default msgs file not found!)");
            return false;
        }

        ro_file_stream nearest_default_entries_file_stream(nearest_default_entries_file_io.get());
        loader::rsc_file nearest_default_entries_loader(reinterpret_cast<common::ro_stream*>(&nearest_default_entries_file_stream));

        auto entries_info_buf = nearest_default_entries_loader.read(1);

        if (entries_info_buf.size() < 2) {
            LOG_ERROR("Default messages file is corrupted, unable to create standard msg entries!");
            return false;
        }

        common::chunkyseri seri(entries_info_buf.data(), entries_info_buf.size(), common::SERI_MODE_READ);
        std::uint16_t entry_count = 0;

        seri.absorb(entry_count);

        for (std::uint16_t i = 0; i < entry_count; i++) {
            epoc::msv::entry ent;

            seri.absorb(ent.id_);

            // Reserve 4 bits for drive ID
            ent.id_ = (ent.id_ & 0x0FFFFFFF) | ((static_cast<std::uint8_t>(crr_drive) & 0xF) << 28);

            seri.absorb(ent.parent_id_);
            seri.absorb(ent.service_id_);
            seri.absorb(ent.type_uid_);
            seri.absorb(ent.mtm_uid_);
            seri.absorb(ent.data_);
            
            loader::absorb_resource_string(seri, ent.description_);
            loader::absorb_resource_string(seri, ent.details_);

            ent.time_ = common::get_current_time_in_microseconds_since_epoch();

            if (!add_entry(ent)) {
                return false;
            }
        }

        return true;
    }
};