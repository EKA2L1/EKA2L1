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
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/version.h>
#include <gdbstub/gdbstub.h>

#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>
#include <dispatch/libraries/register.h>

#include <system/devices.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>

#include <services/window/window.h>

#include <qt/state.h>

namespace eka2l1::desktop {
    static const char *PATCH_FOLDER_PATH = ".//patch//";

    emulator::emulator()
        : symsys(nullptr)
        , graphics_driver(nullptr)
        , audio_driver(nullptr)
        , window(nullptr)
        , joystick_controller(nullptr)
        , should_emu_quit(false)
        , should_emu_pause(false)
        , stage_two_inited(false)
        , first_time(true)
        , init_fullscreen(false)
        , winserv(nullptr)
        , sys_reset_cbh(0) {
    }

    void emulator::stage_one() {
        // Initialize the logger
        log::setup_log(nullptr);

        LOG_INFO(FRONTEND_CMDLINE, "EKA2L1 v0.0.1 ({}-{})", GIT_BRANCH, GIT_COMMIT_HASH);

        // Start to read the configs
        conf.deserialize();
        app_settings = std::make_unique<config::app_settings>(&conf);

        system_create_components comp;
        comp.audio_ = nullptr;
        comp.graphics_ = nullptr;
        comp.conf_ = &conf;
        comp.settings_ = app_settings.get();

        symsys = std::make_unique<eka2l1::system>(comp);

        device_manager *dvcmngr = symsys->get_device_manager();

        if (dvcmngr->total() > 0) {
            symsys->startup();
            
            if (conf.enable_gdbstub) {
                symsys->get_gdb_stub()->set_server_port(conf.gdb_port);
            }

            if (!symsys->set_device(conf.device)) {
                LOG_ERROR(FRONTEND_CMDLINE, "Failed to set a device, device index is out of range (device index in config file is: {})", conf.device);
                LOG_INFO(FRONTEND_CMDLINE, "We are setting the default device back to the first device on the installed list for you");

                conf.device = 0;
                symsys->set_device(0);
            }

            symsys->mount(drive_c, drive_media::physical, eka2l1::add_path(conf.storage, "/drives/c/"), io_attrib_internal);
            symsys->mount(drive_d, drive_media::physical, eka2l1::add_path(conf.storage, "/drives/d/"), io_attrib_internal);
            symsys->mount(drive_e, drive_media::physical, eka2l1::add_path(conf.storage, "/drives/e/"), io_attrib_removeable);
            
            on_system_reset(symsys.get());
            sys_reset_cbh = symsys->add_system_reset_callback([this](system *the_sys) {
                on_system_reset(the_sys);
            });
        }

        first_time = true;
        stage_two_inited = false;
    }

    bool emulator::stage_two() {
        if (!stage_two_inited) {
            device_manager *dvcmngr = symsys->get_device_manager();
            device *dvc = dvcmngr->get_current();

            if (!dvc) {
                LOG_ERROR(FRONTEND_CMDLINE, "No current device is available. Stage two initialisation abort");
                return false;
            }
            
            LOG_INFO(FRONTEND_CMDLINE, "Device being used: {} ({})", dvc->model, dvc->firmware_code);

            // Mount the drive Z after the ROM was loaded. The ROM load than a new FS will be
            // created for ROM purpose.
            symsys->mount(drive_z, drive_media::rom,
                eka2l1::add_path(conf.storage, "/drives/z/"), io_attrib_internal | io_attrib_write_protected);

            // Create audio driver
            audio_driver = drivers::make_audio_driver(drivers::audio_driver_backend::cubeb);
            symsys->set_audio_driver(audio_driver.get());

            // Load patch libraries
            kernel_system *kern = symsys->get_kernel_system();
            hle::lib_manager *libmngr = kern->get_lib_manager();
            dispatch::dispatcher *disp = symsys->get_dispatcher();

            // Start the bootload
            kern->start_bootload();

            libmngr->load_patch_libraries(PATCH_FOLDER_PATH);
            dispatch::libraries::register_functions(kern, disp);

            // Uncomment after cenrep changes
            if (!conf.cenrep_reset) {
                io_system *io = symsys->get_io_system();

                auto private_dir_c_persists = io->get_raw_path(u"C:\\Private\\10202be9\\persists\\");
                auto private_dir_d_persists = io->get_raw_path(u"D:\\Private\\10202be9\\persists\\");
                auto private_dir_e_persists = io->get_raw_path(u"E:\\Private\\10202be9\\persists\\");
                
                common::delete_folder(common::ucs2_to_utf8(*private_dir_c_persists));
                common::delete_folder(common::ucs2_to_utf8(*private_dir_d_persists));
                common::delete_folder(common::ucs2_to_utf8(*private_dir_e_persists));

                conf.cenrep_reset = true;
                conf.serialize();
            }

            manager::packages *pkgmngr = symsys->get_packages();

            pkgmngr->load_registries();
            pkgmngr->migrate_legacy_registries();

            stage_two_inited = true;
        }

        return true;
    }

    void emulator::on_system_reset(system *the_sys) {
        winserv = reinterpret_cast<eka2l1::window_server *>(the_sys->get_kernel_system()->get_by_name<eka2l1::service::server>(
            eka2l1::get_winserv_name_by_epocver(symsys->get_symbian_version_use())));

        // Load patch libraries
        if (stage_two_inited) {
            kernel_system *kern = symsys->get_kernel_system();
            hle::lib_manager *libmngr = kern->get_lib_manager();
            dispatch::dispatcher *disp = the_sys->get_dispatcher();

            // Start the bootload
            kern->start_bootload();

            libmngr->load_patch_libraries(PATCH_FOLDER_PATH);
            dispatch::libraries::register_functions(kern, disp);
        }
    }
}
