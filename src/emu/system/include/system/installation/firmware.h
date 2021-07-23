/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#pragma once

#include <common/types.h>
#include <system/installation/common.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace eka2l1 {
    using device_firmware_choose_variant_callback = std::function<int(const std::vector<std::string> &)>;

    class device_manager;

    device_installation_error install_firmware(device_manager *dvc, const std::string &vpl_path,
        const std::string &drives_c_path, const std::string &drives_e_path, const std::string &drives_z_path,
        const std::string &rom_resident_path, device_firmware_choose_variant_callback choose_callback, progress_changed_callback progress_callback,
        cancel_requested_callback cancel_cb);
}
