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

#include <system/devices.h>
#include <system/installation/rpkg.h>
#include <system/software.h>

#include <vfs/vfs.h>

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace eka2l1::loader {
    static bool extract_file(const std::string &devices_rom_path, FILE *parent, rpkg_entry &ent) {
        std::string file_full_relative = common::ucs2_to_utf8(ent.path.substr(3));
        std::transform(file_full_relative.begin(), file_full_relative.end(), file_full_relative.begin(),
            ::tolower);

        std::string real_path = add_path(add_path(devices_rom_path, "/temp/"), file_full_relative);

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

            if (fread(temp.data(), 1, take, parent) != take) {
                return false;
            }

            if (fwrite(temp.data(), 1, take, wf) != take) {
                return false;
            }

            left -= take;
        }

        fclose(wf);

        return true;
    }

    bool install_rpkg(device_manager *dvcmngr, const std::string &path,
        const std::string &devices_rom_path, std::string &firmware_code_ret, std::atomic<int> &res) {
        FILE *f = fopen(path.data(), "rb");

        if (!f) {
            return false;
        }

        rpkg_header header;

        if (fread(&header.magic, 4, 4, f) != 4) {
            return false;
        }

        std::uint8_t is_ver_one = 1;

        if (header.magic[0] != 'R' || header.magic[1] != 'P' || header.magic[2] != 'K') {
            switch (header.magic[3]) {
            case 'G':
                is_ver_one = 1;
                break;

            case '2':
                is_ver_one = 0;
                break;

            default:
                fclose(f);
                return false;
            }
        }

        std::size_t total_read_size = 0;

        total_read_size += fread(&header.major_rom, 1, 1, f);
        total_read_size += fread(&header.minor_rom, 1, 1, f);
        total_read_size += fread(&header.build_rom, 1, 2, f);
        total_read_size += fread(&header.count, 1, 4, f);

        header.machine_uid = 0;
        header.header_size = 0;

        if (!is_ver_one) {
            total_read_size += fread(&header.header_size, 1, 4, f);
            total_read_size += fread(&header.machine_uid, 1, 4, f);
        }

        if (total_read_size != 8 + header.header_size) {
            fclose(f);
            return false;
        }

        while (!feof(f)) {
            total_read_size = 0;

            rpkg_entry entry;

            total_read_size += fread(&entry.attrib, 1, 8, f);
            total_read_size += fread(&entry.time, 1, 8, f);
            total_read_size += fread(&entry.path_len, 1, 8, f);

            if (total_read_size != 24) {
                break;
            }

            entry.path.resize(entry.path_len);

            total_read_size = 0;

            total_read_size += fread(entry.path.data(), 1, entry.path_len * 2, f);
            total_read_size += fread(&entry.data_size, 1, 8, f);

            if (total_read_size != entry.path_len * 2 + 8) {
                break;
            }

            LOG_INFO("Extracting: {}", common::ucs2_to_utf8(entry.path));

            if (!extract_file(devices_rom_path, f, entry)) {
                break;
            }

            res += (int)(100 / header.count);
        }

        fclose(f);

        const std::string folder_extracted = add_path(devices_rom_path, "temp\\");
        epocver ver = determine_rpkg_symbian_version(folder_extracted);

        std::string manufacturer;
        std::string firmcode;
        std::string model;

        if (!determine_rpkg_product_info(folder_extracted, manufacturer, firmcode, model)) {
            LOG_ERROR("Revert all changes");
            eka2l1::common::remove(add_path(devices_rom_path, "\\temp\\"));

            return false;
        }

        auto firmcode_low = common::lowercase_string(firmcode);
        firmware_code_ret = firmcode_low;

        // Rename temp folder to its product code
        eka2l1::common::move_file(add_path(devices_rom_path, "\\temp\\"), add_path(devices_rom_path, firmcode_low + "\\"));

        if (!dvcmngr->add_new_device(firmcode, model, manufacturer, ver, header.machine_uid)) {
            LOG_ERROR("This device ({}) failed to be install, revert all changes", firmcode);
            eka2l1::common::remove(add_path(devices_rom_path, firmcode_low + "\\"));

            return false;
        }

        return true;
    }
}
