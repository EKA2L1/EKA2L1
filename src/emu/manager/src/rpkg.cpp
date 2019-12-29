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
#include <common/fileutils.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/path.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace eka2l1 {
    namespace loader {
        bool extract_file(const std::string &devices_rom_path, FILE *parent, rpkg_entry &ent) {
            std::string file_full_relative = common::ucs2_to_utf8(ent.path.substr(3));
            std::transform(file_full_relative.begin(), file_full_relative.end(), file_full_relative.begin(),
                ::tolower);

            std::string real_path = add_path(add_path(devices_rom_path, "//temp//"), file_full_relative);

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

        bool install_rpkg(manager::device_manager *dvcmngr, const std::string &path,
            const std::string &devices_rom_path, std::atomic<int> &res) {
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

            epocver ver = epocver::epoc94;

            {
                // We are done extracting, but we need to get the rom info, also
                std::string platform_ini_path = devices_rom_path + "temp\\resource\\versions\\platform.txt";

                common::ini_file platform_ini;

                if (platform_ini.load(platform_ini_path.c_str()) != 0) {
                    LOG_ERROR("Can't load platform.txt in Z:\\Resources\\Versions, default to 9.4");
                } else {
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

                        case 10: {
                            // TODO: Separation
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
                }
            }

            std::string product_ini_path = devices_rom_path + "temp\\resource\\versions\\product.txt";
            common::ini_file product_ini;

            if (product_ini.load(product_ini_path.c_str(), false) != 0) {
                LOG_ERROR("Can't load product.txt in Z:\\Resource\\Versions, revert all changes");
                eka2l1::common::remove(devices_rom_path + "temp\\");

                return false;
            }

            std::string manufacturer = product_ini.find("Manufacturer")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();

            std::string firmcode = product_ini.find("Product")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();

            std::string model = product_ini.find("Model")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();

            if (!dvcmngr->add_new_device(firmcode, model, manufacturer, ver)) {
                LOG_ERROR("This device ({}) already installed, revert all changes", firmcode);
                eka2l1::common::remove(devices_rom_path + "temp\\");

                return false;
            }

            firmcode = common::lowercase_string(firmcode);

            // Rename temp folder to its product code
            eka2l1::common::move_file(devices_rom_path + "temp\\", devices_rom_path + firmcode + "\\");

            return true;
        }
    }
}
