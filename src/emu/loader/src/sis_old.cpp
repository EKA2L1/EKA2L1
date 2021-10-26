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

#include <loader/sis_old.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/buffer.h>

#include <algorithm>
#include <cctype>
#include <cwctype>

#include <miniz.h>

namespace eka2l1::loader {
    std::optional<sis_old> parse_sis_old(const std::string &path) {
        common::ro_std_file_stream stream(path, true);

        if (!stream.valid()) {
            return std::nullopt;
        }

        sis_old sold; // Not like trading anything

        if (stream.read(&sold.header, sizeof(sis_old_header)) != sizeof(sis_old_header)) {
            return std::nullopt;
        }

        sold.epoc_ver = (sold.header.uid2 == static_cast<uint32_t>(epoc_sis_type::epocu6)) ? epocver::epocu6 : epocver::epoc6;
        stream.seek(sold.header.file_ptr, common::seek_where::beg);

        for (uint32_t i = 0; i < sold.header.num_files; i++) {
            sis_old_file_record old_file_record;
            sis_old_file old_file;

            if (stream.read(&old_file_record,sizeof(sis_old_file_record)) != sizeof(sis_old_file_record)) {
                return std::nullopt;
            }

            const std::size_t crr = stream.tell();

            stream.seek(old_file_record.source_name_ptr, common::seek_where::beg);
            old_file.name.resize(old_file_record.source_name_len / 2);

            // Unused actually, we currently don't care
            [[maybe_unused]] std::size_t total_size_read = 0;

            total_size_read = stream.read(old_file.name.data(), old_file_record.source_name_len);

            stream.seek(old_file_record.des_name_ptr, common::seek_where::beg);
            old_file.dest.resize(old_file_record.des_name_len / 2);

            total_size_read = stream.read(old_file.dest.data(), old_file_record.des_name_len);

            old_file.record = std::move(old_file_record);
            sold.files.push_back(std::move(old_file));

            if (sold.epoc_ver != epocver::epoc6) {
                stream.seek(crr - 12, common::seek_where::beg);
            } else {
                stream.seek(crr, common::seek_where::beg);
            }
        }

        for (std::uint32_t i = 0; i < sold.header.num_langs; i++) {
            // Read the language
            std::uint16_t lang = 0;

            stream.seek(sold.header.lang_ptr + i * 2, common::seek_where::beg);
            stream.read(&lang, 2);

            sold.langs.push_back(static_cast<language>(lang));

            // Read component name
            std::u16string name;

            stream.seek(sold.header.comp_name_ptr + i * 4, common::seek_where::beg);

            // Read only one name
            std::uint32_t name_len = 0;
            stream.read(&name_len, 4);

            name.resize(name_len / 2);

            // Read name offset
            stream.seek(sold.header.comp_name_ptr + 4 * sold.header.num_langs + i * 4, common::seek_where::beg);
            std::uint32_t pkg_name_offset = 0;

            stream.read(&pkg_name_offset, 4);
            stream.seek(pkg_name_offset, common::seek_where::beg);
            stream.read(&name[0], name_len);

            sold.comp_names.push_back(name);
        }

        return sold;
    }
}
