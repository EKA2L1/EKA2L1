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

#include <services/fs/std.h>
#include <vfs/vfs.h>

#include <common/cvt.h>

namespace eka2l1::epoc::fs {
    std::string get_server_name_through_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "FileServer";
        }

        return "!FileServer";
    }

    std::uint32_t build_attribute_from_entry_info(entry_info &info) {
        std::uint32_t attrib = epoc::fs::entry_att_normal;

        if (info.has_raw_attribute) {
            attrib = info.raw_attribute;
        } else {
            bool dir = (info.type == io_component_type::dir);

            if (static_cast<int>(info.attribute) & static_cast<int>(io_attrib_write_protected)) {
                attrib |= epoc::fs::entry_att_read_only;
            }

            if (static_cast<int>(info.attribute) & static_cast<int>(io_attrib_hidden)) {
                attrib |= epoc::fs::entry_att_hidden;
            }

            // TODO (pent0): Mark the file as XIP if is ROM image (probably ROM already did it, but just be cautious).

            if (dir) {
                attrib |= epoc::fs::entry_att_dir;
            } else {
                attrib |= epoc::fs::entry_att_archive;
            }
        }

        return attrib;
    }
    
    void build_symbian_entry_from_emulator_entry(io_system *io, entry_info &info, epoc::fs::entry &sym_entry) {
        sym_entry.size = static_cast<std::uint32_t>(info.size);
        sym_entry.size_high = static_cast<std::uint32_t>(info.size >> 32);

        const std::u16string name_ucs2 = common::utf8_to_ucs2(info.name);
        const std::u16string fullpath_ucs2 = common::utf8_to_ucs2(info.full_path);

        sym_entry.attrib = build_attribute_from_entry_info(info);
        sym_entry.name = name_ucs2;
        sym_entry.modified = epoc::time{ info.last_write };

        // Stub some dead value for debugging
        sym_entry.uid1 = sym_entry.uid2 = sym_entry.uid3 = 0xDEADBEEF;
        
        if (info.type == io_component_type::file) {
            // Temporarily open it to read UIDs
            symfile f = io->open_file(fullpath_ucs2, READ_MODE | BIN_MODE);

            if (f) {
                f->read_file(&sym_entry.uid1, sizeof(sym_entry.uid1), 1);
                f->read_file(&sym_entry.uid2, sizeof(sym_entry.uid2), 1);
                f->read_file(&sym_entry.uid3, sizeof(sym_entry.uid3), 1);
            }
        }
    }
}