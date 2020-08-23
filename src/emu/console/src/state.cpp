/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <common/algorithm.h>
#include <common/log.h>
#include <common/path.h>
#include <common/version.h>
#include <console/state.h>
#include <gdbstub/gdbstub.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>

#include <manager/device_manager.h>
#include <manager/manager.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>

#include <services/window/window.h>

namespace eka2l1::desktop {
    void emulator::stage_one() {
        // Initialize the logger
        logger = std::make_shared<imgui_logger>();
        log::setup_log(logger);

        LOG_INFO("EKA2L1 v0.0.1 ({}-{})", GIT_BRANCH, GIT_COMMIT_HASH);

        // Start to read the configs
        conf.deserialize();

        symsys = std::make_unique<eka2l1::system>(nullptr, nullptr, &conf);

        manager::device_manager *dvcmngr = symsys->get_manager_system()->get_device_manager();

        if (dvcmngr->total() > 0) {
            symsys->startup();
            
            if (conf.enable_gdbstub) {
                symsys->get_gdb_stub()->set_server_port(conf.gdb_port);
            }

            if (!symsys->set_device(conf.device)) {
                LOG_ERROR("Failed to set a device, device index is out of range (device index in config file is: {})", conf.device);
                LOG_INFO("We are setting the default device back to the first device on the installed list for you");

                conf.device = 0;
                symsys->set_device(0);
            }
            
            symsys->set_debugger(debugger.get());
            symsys->mount(drive_c, drive_media::physical, eka2l1::add_path(conf.storage, "/drives/c/"), io_attrib::internal);
            symsys->mount(drive_d, drive_media::physical, eka2l1::add_path(conf.storage, "/drives/d/"), io_attrib::internal);
            symsys->mount(drive_e, drive_media::physical, eka2l1::add_path(conf.storage, "/drives/e/"), io_attrib::removeable);
            
            winserv = reinterpret_cast<eka2l1::window_server *>(symsys->get_kernel_system()->get_by_name<eka2l1::service::server>(
                eka2l1::get_winserv_name_by_epocver(symsys->get_symbian_version_use())));
        }

        first_time = true;
        launch_requests.max_pending_count_ = 100;

        // Make debugger. Go watch Case Closed.
        debugger = std::make_unique<eka2l1::imgui_debugger>(symsys.get(), logger.get(), [&](const std::u16string &path, const std::u16string &args) {
            launch_request req { path, args };
            launch_requests.push(req);
        });

        stage_two_inited = false;
    }

    void emulator::stage_two() {
        if (!stage_two_inited) {
            manager::device_manager *dvcmngr = symsys->get_manager_system()->get_device_manager();
            manager::device *dvc = dvcmngr->get_current();

            if (!dvc) {
                LOG_ERROR("No current device is available. Stage two initialisation abort");
                return;
            }
            
            LOG_INFO("Device being used: {} ({})", dvc->model, dvc->firmware_code);

            bool res = symsys->load_rom(add_path(conf.storage, add_path("roms", add_path(
                common::lowercase_string(dvc->firmware_code), "SYM.ROM"))));

            if (!res) {
                return;
            }

            // Mount the drive Z after the ROM was loaded. The ROM load than a new FS will be
            // created for ROM purpose.
            symsys->mount(drive_z, drive_media::rom,
                eka2l1::add_path(conf.storage, "/drives/z/"), io_attrib::internal | io_attrib::write_protected);

            // Create audio driver
            audio_driver = drivers::make_audio_driver(drivers::audio_driver_backend::cubeb);
            symsys->set_audio_driver(audio_driver.get());

            // Load patch libraries
            kernel_system *kern = symsys->get_kernel_system();
            hle::lib_manager *libmngr = kern->get_lib_manager();

            libmngr->load_patch_libraries(".//patch//");

            stage_two_inited = true;
        }
    }
}