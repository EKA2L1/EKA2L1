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

#include <system/installation/raw_dump.h>
#include <system/devices.h>
#include <system/software.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/fileutils.h>
#include <common/path.h>

namespace eka2l1::loader {
    device_installation_error install_raw_dump(device_manager *dvcmngr, const std::string &path_to_dump, const std::string &devices_z_path, std::string &firmware_code,
        std::atomic<int> &res) {
        const std::string temp_path = add_path(devices_z_path, "temp\\");
        // Rename all file to lowercase
        common::copy_folder(path_to_dump, temp_path, common::FOLDER_COPY_FLAG_LOWERCASE_NAME, &res);

        const epocver ver = determine_rpkg_symbian_version(temp_path);
        std::string platform_name, manufacturer, model;

        if (!determine_rpkg_product_info(temp_path, manufacturer, platform_name, model)) {
            return device_installation_determine_product_failure;
        }

        platform_name = common::lowercase_string(platform_name);

        if (!common::move_file(temp_path, add_path(devices_z_path, platform_name + eka2l1::get_separator()))) {
            return device_installation_raw_dump_fail_to_copy;
        }
        
        const add_device_error err_adddvc = dvcmngr->add_new_device(platform_name, model, manufacturer, ver, 0);

        if (err_adddvc != add_device_none) {
            LOG_ERROR(SYSTEM, "This device ({}) failed to be installed!", platform_name);
            
            switch (err_adddvc) {
            case add_device_existed:
                return device_installation_already_exist;

            case add_device_no_language_present:
                return device_installation_no_languages_present;

            default:
                break;
            }

            return device_installation_general_failure;
        }

        firmware_code = platform_name;
        return device_installation_none;
    }
}