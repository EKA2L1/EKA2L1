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
#include <common/path.h>
#include <common/pystr.h>

#include <services/ui/skin/utils.h>
#include <vfs/vfs.h>

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace eka2l1::epoc {
    static constexpr char16_t *SKIN_FOLDER = u"\\private\\10207114\\import\\";

    std::u16string pid_to_string(const epoc::pid skin_pid) {
        std::u16string result;
        result += fmt::format(u"{:0>8x}", skin_pid.first);

        if (skin_pid.second) {
            result += fmt::format(u"{:0>8x}", skin_pid.second);
        }

        return result;
    }

    static epoc::pid string_to_pid(const std::string &str) {
        common::pystr pstr(str);
        epoc::pid result;

        if (str.length() == 8) {
            result.first = pstr.as_int<std::int32_t>(0, 16);            
        } else {
            result.first = pstr.substr(0, 8).as_int<std::int32_t>(0, 16);
            result.second = pstr.substr(8).as_int<std::int32_t>(0, 16);
        }

        return result;
    }
    
    std::optional<epoc::pid> pick_first_skin(eka2l1::io_system *io) {
        for (drive_number drv = drive_a; drv <= drive_z; drv++) {
            if (io->get_drive_entry(drv)) {
                std::u16string skin_folder_path(1, drive_to_char16(drv));
                skin_folder_path += u":";
                skin_folder_path += SKIN_FOLDER;

                auto dir = io->open_dir(skin_folder_path, {}, io_attrib_include_dir);

                if (!dir) {
                    // No skin in this drive. Proceed as usual
                    continue;
                }

                while (auto entry = dir->get_next_entry()) {
                    if (entry->type == io_component_type::dir) {
                        // Check if the name are convertable to hex
                        if ((entry->name.length() != 8) && (entry->name.length() != 16)) {
                            continue;
                        }

                        bool need_skip = false;

                        for (std::size_t i = 0; i < entry->name.length(); i++) {
                            if (!(((entry->name[i] >= '0') && (entry->name[i] <= '9')) || ((entry->name[i] >= 'a') && (entry->name[i] <= 'f'))
                                || ((entry->name[i] >= 'A') && (entry->name[i] <= 'F')))) {
                                need_skip = true;
                                break;
                            }
                        }

                        if (need_skip) {
                            continue;
                        }

                        auto dir_skn = io->open_dir(common::utf8_to_ucs2(eka2l1::add_path(entry->full_path, "/*.skn")), {}, io_attrib_include_file);
                        if (!dir_skn) {
                            continue;
                        }

                        if (dir_skn->peek_next_entry().has_value()) {
                            // Separate this entry name to PID and return
                            return string_to_pid(entry->name);
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }

    std::optional<std::u16string> find_skin_file(eka2l1::io_system *io, const epoc::pid skin_pid) {
        for (drive_number drv = drive_a; drv <= drive_z; drv++) {
            if (io->get_drive_entry(drv)) {
                std::u16string skin_folder_path(1, drive_to_char16(drv));
                skin_folder_path += u":";
                skin_folder_path += SKIN_FOLDER;
                skin_folder_path += pid_to_string(skin_pid);

                // Add filter to find skn files
                skin_folder_path += u"\\*.skn";

                auto dir = io->open_dir(skin_folder_path, {}, io_attrib_include_file);

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