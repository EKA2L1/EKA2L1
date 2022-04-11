/*
 * Copyright (c) 2020 EKA2L1 Team
 *
 * This file is part of EKA2L1 project.
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

#include <system/software.h>

#include <common/dynamicfile.h>
#include <common/fileutils.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>

#include <optional>

namespace eka2l1::loader {
    static bool determine_rpkg_symbian_version_through_platform_file(const std::string &extracted_path, epocver &ver) {
        const std::string platform_ini_path = add_path(extracted_path, "resource\\versions\\platform.txt");
        common::ini_file platform_ini;

        if (platform_ini.load(platform_ini_path.c_str()) != 0) {
            LOG_ERROR(SYSTEM, "Unable to load platform.txt in Z:\\Resources\\Versions.");
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
                    // Note: This should be detected through SIS!
                    ver = epocver::epoc93fp2;
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
                LOG_WARN(SYSTEM, "Unsupport Symbian version {}, will default to 9.4", major, minor);
                break;
            }
            }
        } else {
            LOG_ERROR(SYSTEM, "Symbian platform.txt found, but one of two needed nodes weren't present");
            LOG_ERROR(SYSTEM, "SymbianOSMajorVersion and SymbianOSMinorVersion. Report to developers for investigation.");
        }

        return true;
    }

    static std::optional<epocver> get_epocver_through_sis_name_version(const int major, const int minor) {
        switch (major) {
        case 5:
            if (minor == 0)
                return epocver::epoc94;
            else if (minor < 3)
                return epocver::epoc95;
            else
                return epocver::epoc10;

        case 3:
            if (minor <= 1) {
                return epocver::epoc93fp1;
            }

            return epocver::epoc93fp2;

        case 2:
            if (minor < 8)
                return epocver::epoc80;
            else if (minor == 8)
                return epocver::epoc81a;
            else
                return epocver::epoc81b;

        case 1:
            return epocver::epoc6;

        case 0:
            return epocver::epocu6;

        default:
            break;
        }

        return std::nullopt;
    }

    static bool determine_rpkg_symbian_version_through_series60_sis(const std::string &extracted_path, epocver &ver) {
        const std::string sis_path = add_path(extracted_path, "system\\install\\series60v*.sis");

        auto directory = common::make_directory_iterator(sis_path);
        if (!directory) {
            return false;
        }

        common::dir_entry entry;

        bool found = false;

        while (directory->next_entry(entry) >= 0) {
            if (entry.type == common::FILE_DIRECTORY) {
                continue;
            }

            const common::pystr sis_filename = eka2l1::filename(entry.name);
            auto version_strings = sis_filename.split('.');

            if (version_strings.size() < 2) {
                LOG_ERROR(SYSTEM, "Unable to extract symbian version from second method! Default to v94");
            } else {
                // Try to strip series60v. 9 is the length of that.
                version_strings[0] = version_strings[0].substr(9, std::string::npos);

                const int major = version_strings[0].as_int<int>();
                const int minor = version_strings[1].as_int<int>();

                const std::optional<epocver> discovered = get_epocver_through_sis_name_version(major, minor);
                if (discovered && (!found || (discovered.value() > ver))) {
                    ver = discovered.value();
                    found = true;
                }
            }
        }

        return found;
    }

    epocver determine_rpkg_symbian_version(const std::string &extracted_path) {
        epocver target_ver = epocver::epoc94;

        if (!determine_rpkg_symbian_version_through_series60_sis(extracted_path, target_ver)) {
            LOG_WARN(SYSTEM, "First method determining Symbian version failed, second one begins");

            if (!determine_rpkg_symbian_version_through_platform_file(extracted_path, target_ver)) {
                LOG_ERROR(SYSTEM, "Second method determining Symbian version failed! Default version is 9.4!");
                LOG_INFO(SYSTEM, "You can manually edit the Symbian version in devices.yml after this device successfully installed.");
                LOG_INFO(SYSTEM, "Contact the developer to help improve the automation this process");
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
            LOG_ERROR(SYSTEM, "Failed to load the file.");
            return false;
        }

        manufacturer = product_ini.find("Manufacturer")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();
        firmcode = product_ini.find("Product")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();
        model = product_ini.find("Model")->get_as<common::ini_pair>()->get<common::ini_value>(0)->get_value();

        return true;
    }

    static bool determine_rpkg_product_info_from_various_txts(const std::string &extracted_path,
        std::string &manufacturer, std::string &firmcode, std::string &model) {
        std::string version_folder = add_path(extracted_path, "resource\\versions\\");

        if (!common::exists(version_folder)) {
            version_folder = add_path(extracted_path, "system\\versions\\");
        }

        if (!common::exists(version_folder)) {
            return false;
        }

        common::dynamic_ifile sw_file(add_path(version_folder, "sw.txt"));
        std::string line_buffer;

        if (sw_file.fail()) {
            LOG_ERROR(SYSTEM, "Can't load sw.txt!");
            return false;
        }

        sw_file.getline(line_buffer);

        common::pystr sw_line_py(line_buffer);
        auto sw_infos = sw_line_py.split("\\n");

        firmcode = sw_infos[2].strip_reserverd().std_str();
        manufacturer = "Unknown";

        if (sw_infos.size() >= 4) {
            // Not really... But better then nothing.
            manufacturer = sw_infos[3].strip_reserverd().std_str();
        }

        common::dynamic_ifile model_file(add_path(version_folder, "model.txt"));
        if (model_file.fail()) {
            LOG_ERROR(SYSTEM, "Can't load model.txt, use backup!");
            model = sw_infos[0].std_str();
        } else {
            model_file.getline(model);
        }

        return true;
    }

    bool determine_rpkg_product_info(const std::string &extracted_path, std::string &manufacturer, std::string &firmcode, std::string &model) {
        if (!determine_rpkg_product_info_from_platform_txt(extracted_path, manufacturer, firmcode, model)) {
            LOG_WARN(SYSTEM, "First method of determining product info failed, start the second one.");

            if (!determine_rpkg_product_info_from_various_txts(extracted_path, manufacturer, firmcode, model)) {
                LOG_ERROR(SYSTEM, "Second method of determining product info failed!");
                return false;
            }
        }

        return true;
    }
}