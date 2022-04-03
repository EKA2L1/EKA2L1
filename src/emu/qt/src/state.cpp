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

#include <system/devices.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>

#include <services/window/window.h>
#include <services/init.h>

#include <qt/state.h>
#include <qt/utils.h>

#include <QSettings>

namespace eka2l1::desktop {
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
        , init_app_launched(false)
        , winserv(nullptr)
        , sys_reset_cbh(0)
        , present_status(0) {
    }

    void emulator::stage_one() {
        // Initialize the logger
        log::setup_log(nullptr);

        QSettings settings;
        QVariant cmd_log_enabled_variant = settings.value(SHOW_LOG_CONSOLE_SETTING_NAME, true);

        if (cmd_log_enabled_variant.toBool()) {
            // Initially false. Toggle to set to true!
            log::toggle_console();
        }

        // Start to read the configs
        conf.deserialize();
        if (log::filterings) {
            log::filterings->parse_filter_string(conf.log_filter);
        }

        LOG_INFO(FRONTEND_CMDLINE, "EKA2L1 v0.0.1 ({}-{})", GIT_BRANCH, GIT_COMMIT_HASH);
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
        }

        sys_reset_cbh = symsys->add_system_reset_callback([this](system *the_sys) {
            on_system_reset(the_sys);
        });

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

            drivers::player_type player_be = drivers::player_type_tsf;
            switch (conf.midi_backend) {
            case config::MIDI_BACKEND_MINIBAE:
                player_be = drivers::player_type_minibae;
                break;

            default:
                player_be = drivers::player_type_tsf;
                break;
            }

            // Create audio driver
            audio_driver = drivers::make_audio_driver(drivers::audio_driver_backend::cubeb, conf.audio_master_volume,
                player_be);

            if (audio_driver) {
                audio_driver->set_bank_path(drivers::MIDI_BANK_TYPE_HSB, conf.hsb_bank_path);
                audio_driver->set_bank_path(drivers::MIDI_BANK_TYPE_SF2, conf.sf2_bank_path);
            }

            symsys->set_audio_driver(audio_driver.get());

            // Create sensor driver
            sensor_driver = drivers::sensor_driver::instantiate();
            if (!sensor_driver) {
                LOG_WARN(FRONTEND_CMDLINE, "Failed to create sensor driver");
            }

            symsys->set_sensor_driver(sensor_driver.get());
            symsys->initialize_user_parties();

            io_system *io = symsys->get_io_system();

            // Uncomment after cenrep changes
            if (!conf.cenrep_reset) {
                auto private_dir_c_persists = io->get_raw_path(u"C:\\Private\\10202be9\\persists\\");
                auto private_dir_d_persists = io->get_raw_path(u"D:\\Private\\10202be9\\persists\\");
                auto private_dir_e_persists = io->get_raw_path(u"E:\\Private\\10202be9\\persists\\");

                common::delete_folder(common::ucs2_to_utf8(*private_dir_c_persists));
                common::delete_folder(common::ucs2_to_utf8(*private_dir_d_persists));
                common::delete_folder(common::ucs2_to_utf8(*private_dir_e_persists));

                conf.cenrep_reset = true;
                conf.serialize();
            }

            // Uncomment after MTM reset changes for a while
            if (!conf.mtm_reset) {
                auto private_mtm_c_path = io->get_raw_path(u"C:\\Private\\1000484b\\");

                common::delete_folder(common::ucs2_to_utf8(*private_mtm_c_path));

                conf.mtm_reset = true;
                conf.serialize();
            }

            if (!conf.mtm_reset_2) {
                auto private_mtm_c_path = io->get_raw_path(u"C:\\System\\Mtm\\");

                common::delete_folder(common::ucs2_to_utf8(*private_mtm_c_path));

                conf.mtm_reset_2 = true;
                conf.serialize();
            }

            // Copy additional DLLs
            std::vector<std::tuple<std::u16string, std::string, epocver>> dlls_need_to_copy = {
                { u"Z:\\sys\\bin\\goommonitor.dll", "patch\\goommonitor_general.dll", epocver::epoc94 },
                { u"Z:\\sys\\bin\\avkonfep.dll", "patch\\avkonfep_general.dll", epocver::epoc93fp1 }
            };

            for (std::size_t i = 0; i < dlls_need_to_copy.size(); i++) {
                epocver ver_required = std::get<2>(dlls_need_to_copy[i]);
                if (symsys->get_symbian_version_use() < ver_required) {
                    continue;
                }

                std::u16string org_file_path = std::get<0>(dlls_need_to_copy[i]);
                auto where_to_copy = io->get_raw_path(org_file_path);

                if (where_to_copy.has_value()) {
                    std::string where_to_copy_u8 = common::ucs2_to_utf8(where_to_copy.value());
                    std::string where_to_backup_u8 = where_to_copy_u8 + ".bak";
                    if (common::exists(where_to_copy_u8) && !common::exists(where_to_backup_u8)) {
                        common::move_file(where_to_copy_u8, where_to_copy_u8 + ".bak");
                    }
                    common::copy_file(std::get<1>(dlls_need_to_copy[i]), where_to_copy_u8, true);
                }
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
            symsys->initialize_user_parties();
        }
    }
}
