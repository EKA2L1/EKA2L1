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

#include <manager/installation/raw_dump.h>
#include <manager/device_manager.h>
#include <manager/software.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/fileutils.h>
#include <common/path.h>

namespace eka2l1::loader {
    bool install_raw_dump(manager::device_manager *dvcmngr, const std::string &path_to_dump, const std::string &devices_z_path, std::string &firmware_code,
        std::atomic<int> &res) {
        const epocver ver = determine_rpkg_symbian_version(path_to_dump);
        std::string platform_name, manufacturer, model;

        if (!determine_rpkg_product_info(path_to_dump, manufacturer, platform_name, model)) {
            return false;
        }

        platform_name = common::lowercase_string(platform_name);

        if (!common::copy_folder(path_to_dump + "\\", add_path(devices_z_path, platform_name + "\\"), common::FOLDER_COPY_FLAG_LOWERCASE_NAME, &res)) {
            return false;
        }

        std::uint16_t time_delay_us = 0;
        manager::get_recommended_stat_for_device(ver, time_delay_us);

        if (!dvcmngr->add_new_device(platform_name, model, manufacturer, ver, 0, time_delay_us)) {
            LOG_ERROR("This device ({}) failed to be installed!", platform_name);
            return false;
        }

        firmware_code = platform_name;
        return true;
    }
}