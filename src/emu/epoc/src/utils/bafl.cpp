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

#include <epoc/utils/bafl.h>
#include <epoc/vfs.h>

#include <common/benchmark.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/pystr.h>

namespace eka2l1::utils {
    std::u16string get_nearest_lang_file(io_system *io, const std::u16string &path,
        const language preferred_lang, const drive_number on_drive) {
        //common::benchmarker marker(__FUNCTION__);

        std::u16string cur_dr_dir = std::u16string(1, drive_to_char16(on_drive)) + u":";
        std::u16string new_path = path;

        if (!eka2l1::is_absolute(new_path, cur_dr_dir, true)) {
            new_path = eka2l1::absolute_path(new_path, cur_dr_dir, true);
        } else {
            const std::u16string root = eka2l1::root_name(new_path, true);

            if (root[0] != cur_dr_dir[0]) {
                new_path[0] = cur_dr_dir[0];
            }
        }

        new_path = eka2l1::replace_extension(new_path, u".r00");
        auto rsc_with_ext_exist = [&](const char16_t *ext) -> bool {
            new_path.pop_back();
            new_path.pop_back();

            new_path.push_back(ext[0]);
            new_path.push_back(ext[1]);

            if (io->exist(new_path)) {
                return true;
            }

            return false;
        };

        auto rsc_for_lang_exist = [&](const language lang) -> bool {
            std::u16string num = common::utf8_to_ucs2(common::to_string(static_cast<int>(lang)));
            if (num.length() == 1) {
                num = u'0' + num;
            }

            return rsc_with_ext_exist(num.c_str());
        };

        // Try to get the ideal lang first
        if (rsc_for_lang_exist(preferred_lang)) {
            return new_path;
        }

        // Try to get a file with extension rsc. It's a standard one.
        if (rsc_with_ext_exist(u"sc")) {
            return new_path;
        }

        // The ideal file doesn't exist. Cycles through all languages. Even we understand English but
        // only Abrics resource available, we still accepts the risk.

        // Open a directory filtering, grab the first result
        new_path.pop_back();
        new_path.pop_back();
        new_path += u'*';

        auto dir = io->open_dir(new_path);

        if (!dir) {
            return {};
        }

        auto first_res = dir->get_next_entry();

        if (!first_res) {
            // Despair grows.
            return {};
        }

        return common::utf8_to_ucs2(first_res->full_path);
    }

    bool is_file_compatible_with_language(const std::string &path, const std::string &target_ext, const language ideal_lang) {
        const std::string curr_extension = common::lowercase_string(eka2l1::path_extension(path));
        const std::string ext_low = target_ext;

        if (curr_extension == ext_low) {
            return true;
        } else {
            // Depend on the current language
            common::pystr lang_region(curr_extension.substr(2));
            const std::int32_t lang_int = lang_region.as_int<std::int32_t>();

            if (static_cast<language>(lang_int) == ideal_lang) {
                return true;
            }
        }

        return false;
    }
}
