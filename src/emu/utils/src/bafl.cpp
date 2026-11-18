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

#include <common/path_types.h>
#include <utils/bafl.h>
#include <vfs/vfs.h>

#include <common/benchmark.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/pystr.h>

namespace eka2l1::utils {
    guest_path get_nearest_lang_file(io_system *io, const guest_path &path,
        const language preferred_lang, const drive_number on_drive) {
        //common::benchmarker marker(__FUNCTION__);

        guest_path cur_dr_dir(std::u16string(1, drive_to_char16(on_drive)) + u":");
        guest_path new_path(path);

        new_path = new_path.absolute(cur_dr_dir);
        if (new_path.root_name() != cur_dr_dir) {
            new_path = cur_dr_dir.join(new_path.relative());
        }
        new_path = new_path.with_extension(u".r00");
        auto rsc_with_ext_exist = [&](const char16_t *ext) -> bool {
            new_path = new_path.with_extension(std::u16string(u".r") + ext);
            return io->exist(new_path);
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
        new_path = new_path.with_extension(u".r*");

        auto dir = io->open_dir(new_path);

        if (!dir) {
            return {};
        }

        auto first_res = dir->get_next_entry();

        if (!first_res) {
            // Despair grows.
            return {};
        }

        return first_res->full_path;
    }

    bool is_file_compatible_with_language(const guest_path &path, const std::string &target_ext, const language ideal_lang) {
        const std::string curr_extension = common::ucs2_to_utf8(path.extension().lookup_key());
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
