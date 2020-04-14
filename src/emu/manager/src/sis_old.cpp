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

#include <manager/sis_old.h>

#include <common/cvt.h>
#include <common/log.h>

#include <algorithm>
#include <cctype>
#include <cwctype>

#include <miniz.h>

namespace eka2l1::loader {
    std::optional<sis_old> parse_sis_old(const std::string &path) {
        FILE *f = fopen(path.c_str(), "rb");

        if (!f) {
            return std::nullopt;
        }

        sis_old sold; // Not like trading anything

        if (fread(&sold.header, 1, sizeof(sis_old_header), f) != sizeof(sis_old_header)) {
            return std::nullopt;
        }

        sold.epoc_ver = (sold.header.uid2 == static_cast<uint32_t>(epoc_sis_type::epocu6)) ? epocver::epocu6 : epocver::epoc6;

        fseek(f, sold.header.file_ptr, SEEK_SET);

        for (uint32_t i = 0; i < sold.header.num_files; i++) {
            sis_old_file_record old_file_record;
            sis_old_file old_file;

            if (fread(&old_file_record, 1, sizeof(sis_old_file_record), f) != sizeof(sis_old_file_record)) {
                return std::nullopt;
            }

            uint32_t crr = ftell(f);

            fseek(f, old_file_record.source_name_ptr, SEEK_SET);
            old_file.name.resize(old_file_record.source_name_len / 2);

            // Unused actually, we currently don't care
            [[maybe_unused]] std::size_t total_size_read = 0;

            total_size_read = fread(old_file.name.data(), 2, old_file_record.source_name_len / 2, f);

            fseek(f, old_file_record.des_name_ptr, SEEK_SET);
            old_file.dest.resize(old_file_record.des_name_len / 2);

            total_size_read = fread(old_file.dest.data(), 2, old_file_record.des_name_len / 2, f);

            old_file.record = std::move(old_file_record);
            sold.files.push_back(std::move(old_file));

            if (sold.epoc_ver != epocver::epoc6) {
                fseek(f, crr - 12, SEEK_SET);
            } else {
                fseek(f, crr, SEEK_SET);
            }
        }

        for (std::uint32_t i = 0; i < sold.header.num_langs; i++) {
            // Read the language
            std::uint16_t lang = 0;
            fseek(f, sold.header.lang_ptr + i * 2, SEEK_SET);
            fread(&lang, 2, 1, f);

            sold.langs.push_back(static_cast<language>(lang));

            // Read component name
            std::u16string name;

            fseek(f, sold.header.comp_name_ptr + i * 4, SEEK_SET);

            // Read only one name
            std::uint32_t name_len = 0;
            fread(&name_len, 4, 1, f);

            name.resize(name_len);

            // Read name offset
            fseek(f, sold.header.comp_name_ptr + 4 * sold.header.num_langs + i * 4, SEEK_SET);
            std::uint32_t pkg_name_offset = 0;

            fread(&pkg_name_offset, 4, 1, f);
            fseek(f, pkg_name_offset, SEEK_SET);
            fread(&name[0], name_len, 1, f);

            sold.comp_names.push_back(name);
        }

        return sold;
    }
}
