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

#include <common/path_types.h>
#include <common/types.h>
#include <system/installation/common.h>
#include <system/installation/firmware.h>

#include <string>
#include <vector>

namespace eka2l1 {
    class device_manager;

    namespace loader {
        /**
         * @brief Install a device from an archive (.7z, .zip, .rar or anything else libarchive reads).
         *
         * Three ways of packing a device are recognised, because all are what people actually share:
         *
         *  - A ROM image next to the RPKG that goes with it, at any depth in the archive. Both are
         *    unpacked to a scratch folder and handed to the regular ROM/RPKG installer.
         *  - An already-unpacked device, laid out the way the emulator stores one: a drive Z tree under
         *    `data/drives/z/<firmware code>/` and the ROM image under `data/roms/<firmware code>/`. Here
         *    the files go straight where they belong, so nothing has to be dumped out of the ROM again.
         *
         *  - Nokia flash files: VPLs next to the FPSX images they list. When the archive holds more than
         *    one variant, `choose_variant_cb` picks one (by default the first) from the descriptions
         *    list_archive_firmware_variants() reports, in the same order.
         *
         * The device is registered in devices.yml on success, exactly as the other installers do. Which
         * layout the archive uses is decided from its file list, so the caller does not have to care.
         *
         * @param path                    Path to the archive.
         * @param rom_resident_path       The emulator's `roms/` folder.
         * @param drives_z_resident_path  The emulator's `drives/z/` folder.
         * @param isolate_drives          Give the device drives C, D and E of its own.
         */
        device_installation_error install_archive(device_manager *dvc, const host_path &path,
            const host_path &rom_resident_path, const host_path &drives_z_resident_path,
            const bool isolate_drives, progress_changed_callback progress_cb, cancel_requested_callback cancel_cb,
            device_firmware_choose_variant_callback choose_variant_cb = nullptr);

        /**
         * @brief List the firmware variants install_archive() can install from an archive of flash files.
         *
         * @returns device_installation_none with an empty list when the archive holds no VPL at all.
         */
        device_installation_error list_archive_firmware_variants(const host_path &path,
            std::vector<std::string> &descriptions);
    }
}
