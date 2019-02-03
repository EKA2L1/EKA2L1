/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/buffer.h>
#include <common/log.h>

#include <epoc/services/ecom/ecom.h>
#include <epoc/vfs.h>

namespace eka2l1 {
    ecom_server::ecom_server(eka2l1::system *sys)
        : service::server(sys, "!ecomserver", true) {
    }

    std::vector<std::string> ecom_server::get_ecom_plugin_archives(eka2l1::io_system *io) {
        std::u16string pattern = u"";

        // Get ROM drive first
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            auto res = io->get_drive_entry(drv);
            if (res && res->media_type == drive_media::rom) {
                pattern += drive_to_char16(drv);
            }
        }

        if (pattern.empty()) {
            // For some reason, ROM hasn't been mounted yet, break!
            return {};
        }

        pattern += u":\\Private\\10009d8f\\ecom-*-*.s*";
        auto ecom_private_dir = io->open_dir(pattern, io_attrib::none);

        if (!ecom_private_dir) {
            // Again folder not found or error from our side
            LOG_ERROR("Private directory of ecom can't be accessed");
            return {};
        }

        std::vector<std::string> results;

        while (auto entry = ecom_private_dir->get_next_entry()) {
            results.push_back(entry->full_path);
        }

        return results;
    } 
}