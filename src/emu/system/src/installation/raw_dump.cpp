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
    bool install_raw_dump(device_manager *dvcmngr, const std::string &path_to_dump, const std::string &devices_z_path, std::string &firmware_code,
        std::atomic<int> &res) {
        const std::string temp_path = add_path(devices_z_path, "temp\\");
        // Rename all file to lowercase
        common::copy_folder(path_to_dump, temp_path, common::FOLDER_COPY_FLAG_LOWERCASE_NAME, nullptr);

        const epocver ver = determine_rpkg_symbian_version(temp_path);
        std::string platform_name, manufacturer, model;

        if (!determine_rpkg_product_info(temp_path, manufacturer, platform_name, model)) {
            return false;
        }

        platform_name = common::lowercase_string(platform_name);

        if (!common::move_file(temp_path, add_path(devices_z_path, platform_name + eka2l1::get_separator()))) {
            return false;
        }

        if (!dvcmngr->add_new_device(platform_name, model, manufacturer, ver, 0)) {
            LOG_ERROR("This device ({}) failed to be installed!", platform_name);
            return false;
        }

        firmware_code = platform_name;
        return true;
    }
}