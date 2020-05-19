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

#include <manager/device_manager.h>
#include <manager/rpkg.h>

#include <epoc/vfs.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/dynamicfile.h>
#include <common/fileutils.h>
#include <common/ini.h>
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

    static bool determine_rpkg_symbian_version_through_platform_file(const std::string &extracted_path, epocver &ver) {
        const std::string platform_ini_path = add_path(extracted_path, "resource\\versions\\platform.txt");
        common::ini_file platform_ini;

        if (platform_ini.load(platform_ini_path.c_str()) != 0) {
            LOG_ERROR("Unable to load platform.txt in Z:\\Resources\\Versions.");
            return false;
        }

        // Parse the SIS
        std::uint8_t major = 9;
        std::uint8_t minor = 4;

        auto major_node = platform_ini.find("SymbianOSMajorVersion");
        auto minor_node = platform_ini.find("SymbianOSMinorVersion");

        if (major_node && minor_node) {
            major = major_node->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_as_native<std::uint8_t>();
            minor = minor_node->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_as_native<std::uint8_t>();

            switch (major) {
            case 5: {
                // TODO: Separation
                ver = epocver::epocu6;
                break;
            }

            case 6: {
                ver = epocver::epoc6;
                break;
            }

            case 9: {
                switch (minor) {
                case 3: {
                    ver = epocver::epoc93;
                    break;
                }

                case 4: {
                    ver = epocver::epoc94;
                    break;
                }
                }

                break;
            }

            case 100:
            case 101: {
                ver = epocver::epoc10;
                break;
            }

            default: {
                LOG_WARN("Unsupport Symbian version {}, will default to 9.4", major, minor);
                break;
            }
            }
        } else {
            LOG_ERROR("Symbian platform.txt found, but one of two needed nodes weren't present");
            LOG_ERROR("SymbianOSMajorVersion and SymbianOSMinorVersion. Report to developers for investigation.");
        }

        return true;
    }

    static bool determine_rpkg_symbian_version_through_series60_sis(const std::string &extracted_path, epocver &ver) {
        const std::string sis_path = add_path(extracted_path, "system\\install\\series60v*.sis");

        common::dir_iterator directory(sis_path);
        common::dir_entry entry;

        if (directory.next_entry(entry) >= 0) {
            const common::pystr sis_filename = eka2l1::filename(entry.name);
            auto version_strings = sis_filename.split('.');

            if (version_strings.size() < 2) {
                LOG_ERROR("Unable to extract symbian version from second method! Default to v94");
            } else {
                // Try to strip series60v. 9 is the length of that.
                version_strings[0] = version_strings[0].substr(9, std::string::npos);

                const int major = version_strings[0].as_int<int>();
                const int minor = version_strings[1].as_int<int>();

                if (major == 5) {
                    ver = epocver::epoc94;
                } else if (major == 3) {
                    ver = epocver::epoc93;
                }

                return true;
            }
        }

        return false;
    }

    static epocver determine_rpkg_symbian_version(const std::string &extracted_path) {
        epocver target_ver = epocver::epoc94;

        if (!determine_rpkg_symbian_version_through_platform_file(extracted_path, target_ver)) {
            LOG_WARN("First method determining Symbian version failed, second one begins");

            if (!determine_rpkg_symbian_version_through_series60_sis(extracted_path, target_ver)) {
                LOG_ERROR("Second method determining Symbian version failed! Default version is 9.4!");
                LOG_INFO("You can manually edit the Symbian version in devices.yml after this device successfully installed.");
                LOG_INFO("Contact the developer to help improve the automation this process");
            }
        }

        return target_ver;
    }

    static bool determine_rpkg_product_info_from_platform_txt(const std::string &extracted_path,
        std::string &manufacturer, std::string &firmcode, std::string &model) {
        const std::string version_folder = add_path(extracted_path, "resource\\versions\\");
        std::string product_ini_path = add_path(version_folder, "product.txt");
        common::ini_file product_ini;

        if (product_ini.load(product_ini_path.c_str(), false) != 0) {
            LOG_ERROR("Failed to load the file.");
            return false;
        }

        manufacturer = product_ini.find("Manufacturer")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();
        firmcode = product_ini.find("Product")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();
        model = product_ini.find("Model")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();

        return true;
    }

    static bool determine_rpkg_product_info_from_various_txts(const std::string &extracted_path,
        std::string &manufacturer, std::string &firmcode, std::string &model) {
        const std::string version_folder = add_path(extracted_path, "resource\\versions\\");

        common::dynamic_ifile sw_file(add_path(version_folder, "sw.txt"));
        std::string line_buffer;

        if (sw_file.fail()) {
            LOG_ERROR("Can't load sw.txt!");
            return false;
        } else {
            sw_file.getline(line_buffer);

            common::pystr sw_line_py(line_buffer);
            auto sw_infos = sw_line_py.split("\\n");

            firmcode = sw_infos[2].std_str();
            manufacturer = "Unknown";

            if (sw_infos.size() >= 4) {
                // Not really... But better then nothing.
                manufacturer = sw_infos[3].std_str();
            }
        }

        common::dynamic_ifile model_file(add_path(version_folder, "model.txt"));
        if (model_file.fail()) {
            LOG_ERROR("Can't load model.txt!");
            return false;
        } else {
            model_file.getline(model);
        }

        return true;
    }

    static bool determine_rpkg_product_info(const std::string &extracted_path,
        std::string &manufacturer, std::string &firmcode, std::string &model) {
        if (!determine_rpkg_product_info_from_platform_txt(extracted_path, manufacturer,
                firmcode, model)) {
            LOG_WARN("First method of determining product info failed, start the second one.");

            if (!determine_rpkg_product_info_from_various_txts(extracted_path, manufacturer,
                    firmcode, model)) {
                LOG_ERROR("Second method of determining product info failed!");
                return false;
            }
        }

        return true;
    }

    bool install_rpkg(manager::device_manager *dvcmngr, const std::string &path,
        const std::string &devices_rom_path, std::string &firmware_code_ret, std::atomic<int> &res) {
        FILE *f = fopen(path.data(), "rb");

        if (!f) {
            return false;
        }

        rpkg_header header;

        if (fread(&header.magic, 4, 4, f) != 4) {
            return false;
        }

        if (header.magic[0] != 'R' || header.magic[1] != 'P' || header.magic[2] != 'K' || header.magic[3] != 'G') {
            fclose(f);
            return false;
        }

        std::size_t total_read_size = 0;

        total_read_size += fread(&header.major_rom, 1, 1, f);
        total_read_size += fread(&header.minor_rom, 1, 1, f);
        total_read_size += fread(&header.build_rom, 1, 2, f);
        total_read_size += fread(&header.count, 1, 4, f);

        if (total_read_size != 8) {
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

        if (!dvcmngr->add_new_device(firmcode, model, manufacturer, ver)) {
            LOG_ERROR("This device ({}) failed to be install, revert all changes", firmcode);
            eka2l1::common::remove(add_path(devices_rom_path, firmcode_low + "\\"));

            return false;
        }

        return true;
    }
}
