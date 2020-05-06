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
#include <common/time.h>

#include <epoc/services/msv/entry.h>
#include <epoc/vfs.h>
#include <epoc/loader/rsc.h>

#include <epoc/utils/bafl.h>
#include <epoc/utils/err.h>

namespace eka2l1::epoc::msv {
    entry_indexer::entry_indexer(io_system *io, const language preferred_lang)
        : io_(io)
        , rom_drv_(drive_z)
        , preferred_lang_(preferred_lang) {
        drive_number drv_target = drive_z;

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            auto drv_entry = io->get_drive_entry(drv);

            if (drv_entry && (drv_entry->media_type == drive_media::rom)) {
                rom_drv_ = drv;
                break;
            }
        }

        create_standard_entries(static_cast<drive_number>(1));
    }

    bool entry_indexer::add_entry(entry &ent) {
        return true;
    }

    bool entry_indexer::create_root_entry() {
        epoc::msv::entry root_entry;
        root_entry.id_ = 0x1000;
        root_entry.mtm_uid_ = 0x10000F67;
        root_entry.type_uid_ = 0x10000F67;
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