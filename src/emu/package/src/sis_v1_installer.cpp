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
#include <package/sis_v1_installer.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/flate.h>
#include <common/log.h>
#include <common/path.h>

#include <vfs/vfs.h>

#include <cwctype>

namespace eka2l1::loader {
    bool install_sis_old(const std::u16string &path, io_system *io, drive_number drive,
        manager::package_info &info, std::vector<std::u16string> &package_files) {
        loader::sis_old res = *loader::parse_sis_old(common::ucs2_to_utf8(path));

        info.drive = drive;
        info.id = res.header.uid1;
        info.vendor_name = u"Nokia";

        // Pick first one
        info.name = res.comp_names[0];

        for (auto &file : res.files) {
            std::u16string dest = file.dest;

            if (dest.find(u"!") != std::u16string::npos) {
                dest.replace(0, 1, (info.drive == 0) ? u"c" : u"e");
            }

            if (file.record.file_type != 1 && dest != u"") {
                std::string rp = eka2l1::file_directory(common::ucs2_to_utf8(dest));
                io->create_directories(common::utf8_to_ucs2(rp));
            } else {
                continue;
            }

            package_files.push_back(dest);
            symfile f = io->open_file(dest, WRITE_MODE | BIN_MODE);

            LOG_TRACE(PACKAGE, "Installing file {}", common::ucs2_to_utf8(dest));

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

        return false;
    }
}
