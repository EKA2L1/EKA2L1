/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <epoc/loader/sis_old.h>
#include <epoc/vfs.h>

#include <common/cvt.h>
#include <common/log.h>

#include <algorithm>
#include <cctype>
#include <cwctype>

#include <miniz.h>

namespace eka2l1::loader {
    std::optional<sis_old> parse_sis_old(const std::string path) {
        FILE *f = fopen(path.c_str(), "rb");

        if (!f) {
            return std::optional<sis_old>{};
        }

        sis_old sold; // Not like trading anything

        fread(&sold.header, 1, sizeof(sis_old_header), f);

        sold.epoc_ver = (sold.header.uid2 == static_cast<uint32_t>(epoc_sis_type::epocu6)) ? epocver::epocu6 : epocver::epoc6;

        fseek(f, sold.header.file_ptr, SEEK_SET);

        for (uint32_t i = 0; i < sold.header.num_files; i++) {
            sis_old_file_record old_file_record;
            sis_old_file old_file;

            fread(&old_file_record, 1, sizeof(sis_old_file_record), f);

            uint32_t crr = ftell(f);

            fseek(f, old_file_record.source_name_ptr, SEEK_SET);
            old_file.name.resize(old_file_record.source_name_len / 2);

            fread(old_file.name.data(), 2, old_file_record.source_name_len / 2, f);

            fseek(f, old_file_record.des_name_ptr, SEEK_SET);
            old_file.dest.resize(old_file_record.des_name_len / 2);

            fread(old_file.dest.data(), 2, old_file_record.des_name_len / 2, f);

            if (old_file.dest.find(u".app") != std::u16string::npos || old_file.dest.find(u".APP") != std::u16string::npos) {
                sold.app_path = old_file.dest;
            }

            if (old_file.dest.find(u".exe") != std::u16string::npos || old_file.dest.find(u".EXE") != std::u16string::npos) {
                sold.exe_path = old_file.dest;
            }

            old_file.record = std::move(old_file_record);

            sold.files.push_back(std::move(old_file));

            if (sold.epoc_ver != epocver::epoc6) {
                fseek(f, crr - 12, SEEK_SET);
            } else {
                fseek(f, crr, SEEK_SET);
            }
        }

        return sold;
    }
}