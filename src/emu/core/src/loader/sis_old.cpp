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

#include <core/loader/sis_old.h>
#include <core/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/flate.h>
#include <common/path.h>

#include <algorithm>
#include <cctype>

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

    bool install_sis_old(std::u16string path, io_system *io, uint8_t drive, epocver sis_ver, manager::app_info &info) {
        loader::sis_old res = *loader::parse_sis_old(common::ucs2_to_utf8(path));
        std::u16string main_path = res.app_path ? *res.app_path : (res.exe_path ? *res.exe_path : u"");

        std::transform(main_path.begin(), main_path.end(), main_path.begin(), std::towlower);

        if (main_path != u"") {
            if (main_path.find(u"!") == std::u16string::npos) {
                if (main_path.find(u"C:") != std::u16string::npos || main_path.find(u"c:") != std::u16string::npos) {
                    info.drive = 0;
                } else {
                    info.drive = 1;
                }
            } else {
                info.drive = drive;
            }

            size_t last_slash_pos = main_path.find_last_of(u"\\");
            size_t sub_pos = res.app_path ? main_path.find(u".app") : main_path.find(u".exe");

            std::u16string name = (last_slash_pos != std::u16string::npos) ? main_path.substr(last_slash_pos + 1, sub_pos - last_slash_pos - 1) : u"Unknown";

            info.executable_name = main_path;
            info.id = res.header.uid1;
            info.vendor_name = u"Your mom";
            info.name = name;
            info.executable_name = main_path.substr(last_slash_pos + 1);
            info.ver = sis_ver;

            for (auto &file : res.files) {
                std::u16string dest = file.dest;

                if (dest.find(u"!") != std::u16string::npos) {
                    dest.replace(0, 1, (info.drive == 0) ? u"c" : u"e");
                }

                if (file.record.file_type != 1 && dest != u"") {
                    std::string rp = eka2l1::file_directory(io->get(common::ucs2_to_utf8(dest)));
                    eka2l1::create_directories(rp);
                } else {
                    continue;
                }

                symfile f = io->open_file(dest, WRITE_MODE | BIN_MODE);

                LOG_TRACE("Installing file {}", common::ucs2_to_utf8(dest));

                size_t left = file.record.len;
                size_t chunk = 0x2000;

                std::vector<char> temp;
                temp.resize(chunk);

                std::vector<char> inflated;
                inflated.resize(0x100000);

                FILE *sis_file = fopen(common::ucs2_to_utf8(path).data(), "rb");
                fseek(sis_file, file.record.ptr, SEEK_SET);

                mz_stream stream;

                stream.zalloc = nullptr;
                stream.zfree = nullptr;

                if (!(res.header.op & 0x8)) {
                    if (inflateInit(&stream) != MZ_OK) {
                        return false;
                    }
                }

                while (left > 0) {
                    size_t took = left < chunk ? left : chunk;
                    size_t readed = fread(temp.data(), 1, took, sis_file);

                    if (readed != took) {
                        fclose(sis_file);
                        f->close();

                        return false;
                    }

                    if (res.header.op & 0x8)
                        f->write_file(temp.data(), 1, static_cast<uint32_t>(took));
                    else {
                        uint32_t inf;
                        bool res = flate::inflate_data(&stream, temp.data(), inflated.data(), static_cast<uint32_t>(took), &inf);
                        f->write_file(inflated.data(), 1, inf);
                    }

                    left -= took;
                }

                if (!(res.header.op & 0x8))
                    inflateEnd(&stream);
            }
        }

        return false;
    }
}