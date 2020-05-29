/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/cvt.h>

#include <services/akn/skin/utils.h>
#include <vfs/vfs.h>

#include <fmt/format.h>

namespace eka2l1::epoc {
    static std::u16string pid_to_string(const epoc::pid skin_pid) {
        std::u16string result;
        result += fmt::format(u"{:0>8x}", skin_pid.first);

        if (skin_pid.second) {
            result += fmt::format(u"{:0>8x}", skin_pid.second);
        }

        return result;
    }

    std::optional<std::u16string> find_skin_file(eka2l1::io_system *io, const epoc::pid skin_pid) {
        const std::u16string skins_folder(u"\\private\\10207114\\import\\");

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            if (io->get_drive_entry(drv)) {
                std::u16string skin_folder_path(1, drive_to_char16(drv));
                skin_folder_path += u":";
                skin_folder_path += skins_folder;
                skin_folder_path += pid_to_string(skin_pid);

                // Add filter to find skn files
                skin_folder_path += u"\\*.skn";

                auto dir = io->open_dir(skin_folder_path, io_attrib::include_file);

                if (!dir) {
                    // No skin in this drive. Proceed as usual
                    continue;
                }

                // Pick the first one that's not a folder
                while (auto entry = dir->get_next_entry()) {
                    if (entry->type == io_component_type::file) {
                        return common::utf8_to_ucs2(entry->full_path);
                    }
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::u16string> get_resource_path_of_skin(eka2l1::io_system *io, const epoc::pid skin_pid) {
        std::u16string formal_path = u"\\resource\\skins\\";
        formal_path += pid_to_string(skin_pid);

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            if (io->get_drive_entry(drv)) {
                std::u16string skin_folder_path(1, drive_to_char16(drv));
                skin_folder_path += u":";
                skin_folder_path += formal_path;
                skin_folder_path += u"\\";

                auto info = io->get_entry_info(skin_folder_path);

                if (!info) {
                    // No directory on this drive. Continue
                    continue;
                }

                return skin_folder_path;
            }
        }

        return std::nullopt;
    }
}