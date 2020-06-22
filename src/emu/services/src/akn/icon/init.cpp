/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <kernel/libmanager.h>
#include <services/akn/icon/common.h>
#include <services/akn/icon/icon.h>
#include <services/fbs/fbs.h>

#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <vfs/vfs.h>

#include <common/log.h>
#include <loader/rsc.h>

namespace eka2l1 {
    void akn_icon_server::init_server() {
        fbss = reinterpret_cast<fbs_server *>(&(*sys->get_kernel_system()->get_by_name<service::server>(
            epoc::get_fbs_server_name_by_epocver(sys->get_symbian_version_use()))));

        // Get the ROM device, and read the rsc
        eka2l1::io_system *io = sys->get_io_system();
        std::u16string path;

        // Find the ROM drive
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(drv - 1)) {
            auto result = io->get_drive_entry(drv);
            if (result && result->media_type == drive_media::rom) {
                path += drive_to_char16(drv);
                break;
            }
        }

        path += u":\\resource\\akniconsrv.rsc";
        symfile config_file = io->open_file(path, READ_MODE | BIN_MODE);

        if (!config_file) {
            LOG_ERROR("Can't find akniconsrv.rsc! Initialisation failed!");
            return;
        }

        // Read the file
        eka2l1::ro_file_stream config_file_stream(config_file.get());
        loader::rsc_file config_rsc(reinterpret_cast<common::ro_stream *>(&config_file_stream));

        // Read the initialisation data
        // The RSC data are layout as follow:
        // ---------------------------------------------------------------------------------------------
        //    Index   | Size  |       Usage                   |       Note                             |
        //      1     |  1    |    Compression type           |  0 = No, 1 = RLE, 2 = Palette          |
        //      2     |  4    |    Preferred icon depth       |  0 = 64K color, 2 = 16K MU, else 16KM  |
        //      3     |  4    |    Preferred icon mask depth  |  0 = Gray2, else Gray256               |
        //      4     |  4    |    Photo depth                |  Same as icon depth                    |
        //      5     |  4    |    Video depth                |  Same as icon depth                    |
        //      6     |  4    |    Offscreen depth            |  Same as icon depth                    |
        //      7     |  4    |    Offscreen mask depth       |  Same as icon mask depth               |
        // ---------------------------------------------------------------------------------------------
        // More fields to come

        auto read_config_depth_to_display_mode = [&](const int idx) -> epoc::display_mode {
            auto data = config_rsc.read(idx);

            if (data.size() != 4) {
                LOG_ERROR("Try reading config depth, but size of resource is not equal to 4");
            } else {
                switch (*reinterpret_cast<std::uint32_t *>(&data[0])) {
                case 0: {
                    return epoc::display_mode::color64k;
                }

                case 2: {
                    return epoc::display_mode::color16mu;
                }

                default:
                    break;
                }
            }

            return epoc::display_mode::color16m;
        };

        auto read_config_mask_depth_to_display_mode = [&](const int idx) -> epoc::display_mode {
            auto data = config_rsc.read(idx);

            if (data.size() != 4) {
                LOG_ERROR("Try reading config mask depth, but size of resource is not equal to 4");
            } else {
                if (*reinterpret_cast<std::uint32_t *>(&data[0]) == 0) {
                    return epoc::display_mode::gray2;
                }
            }

            return epoc::display_mode::gray256;
        };

        init_data.compression = config_rsc.read(1)[0];
        init_data.icon_mode = read_config_depth_to_display_mode(2);
        init_data.icon_mask_mode = read_config_mask_depth_to_display_mode(3);
        init_data.photo_mode = read_config_depth_to_display_mode(4);
        init_data.video_mode = read_config_depth_to_display_mode(5);
        init_data.offscreen_mode = read_config_depth_to_display_mode(6);
        init_data.offscreen_mask_mode = read_config_mask_depth_to_display_mode(7);

        // Done reading
        // Set the flags to inited.
        flags |= akn_icon_srv_flag_inited;

        static constexpr std::uint32_t AKN_ICON_SRV_UID = 0x1020735B;

        // Create icon process
        icon_process = std::make_unique<service::faker>(sys->get_kernel_system(), sys->get_lib_manager(),
            "AknIconServerProcess", AKN_ICON_SRV_UID);
    }
};
