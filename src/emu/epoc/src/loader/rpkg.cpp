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

#include <epoc/loader/rpkg.h>
#include <epoc/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <array>
#include <vector>

namespace eka2l1 {
    namespace loader {
        bool extract_file(io_system *io, FILE *parent, rpkg_entry &ent) {
            auto path_ucs16_real = io->get_raw_path(ent.path);

            if (!path_ucs16_real) {
                LOG_WARN("Drive Z: not mounted");
                return false;
            }

            std::string real_path = common::ucs2_to_utf8(*path_ucs16_real);

            std::string dir = eka2l1::file_directory(real_path);
            eka2l1::create_directories(dir);

            FILE *wf
                = fopen(real_path.c_str(), "wb");

            if (!wf) {
                LOG_INFO("Skipping with real path: {}, dir: {}", real_path, dir);
                return false;
            }

            int64_t left = ent.data_size;
            int64_t take_def = 0x10000;

            std::array<char, 0x10000> temp;

            while (left) {
                int64_t take = left < take_def ? left : take_def;
                fread(temp.data(), 1, take, parent);
                fwrite(temp.data(), 1, take, wf);

                left -= take;
            }

            fclose(wf);

            return true;
        }

        bool install_rpkg(io_system *io, const std::string &path, std::atomic<int> &res) {
            FILE *f = fopen(path.data(), "rb");

            if (!f) {
                return false;
            }

            rpkg_header header;

            fread(&header.magic, 4, 4, f);

            if (header.magic[0] != 'R' || header.magic[1] != 'P' || header.magic[2] != 'K' || header.magic[3] != 'G') {
                fclose(f);
                return false;
            }

            fread(&header.major_rom, 1, 1, f);
            fread(&header.minor_rom, 1, 1, f);
            fread(&header.build_rom, 1, 2, f);
            fread(&header.count, 1, 4, f);

            while (!feof(f)) {
                rpkg_entry entry;

                fread(&entry.attrib, 1, 8, f);
                fread(&entry.time, 1, 8, f);
                fread(&entry.path_len, 1, 8, f);

                entry.path.resize(entry.path_len);

                fread(entry.path.data(), 1, entry.path_len * 2, f);
                fread(&entry.data_size, 1, 8, f);

                LOG_INFO("Extracting: {}", common::ucs2_to_utf8(entry.path));

                if (!extract_file(io, f, entry)) {
                    fclose(f);
                    return true;
                }

                res += (int)(100 / header.count);
            }

            fclose(f);

            return true;
        }
    }
}