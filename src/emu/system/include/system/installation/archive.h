/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <string>

namespace eka2l1 {
    class device_manager;

    namespace loader {
        /**
         * @brief Install a device from an archive (today a .7z; the reader takes whatever libarchive can).
         *
         * Two ways of packing a device are recognised, because both are what people actually share:
         *
         *  - A ROM image next to the RPKG that goes with it, at any depth in the archive. Both are
         *    unpacked to a scratch folder and handed to the regular ROM/RPKG installer.
         *  - An already-unpacked device, laid out the way the emulator stores one: a drive Z tree under
         *    `data/drives/z/<firmware code>/` and the ROM image under `data/roms/<firmware code>/`. Here
         *    the files go straight where they belong, so nothing has to be dumped out of the ROM again.
         *
         * The device is registered in devices.yml on success, exactly as the other installers do. Which
         * layout the archive uses is decided from its file list, so the caller does not have to care.
         *
         * @param path                    Path to the archive.
         * @param rom_resident_path       The emulator's `roms/` folder.
         * @param drives_z_resident_path  The emulator's `drives/z/` folder.
         */
        device_installation_error install_archive(device_manager *dvc, const std::string &path,
            const std::string &rom_resident_path, const std::string &drives_z_resident_path,
            progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);
    }
}
