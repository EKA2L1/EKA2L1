/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>

namespace eka2l1 {
    class memory_system;
    class kernel_system;
    class device_manager;
    class io_system;
    class ntimer;
    class disasm;
    class gdbstub;
    struct apa_app_registry;

    namespace common {
        class chunkyseri;
    }

    namespace epoc {
        struct hal;
    }

    namespace hle {
        class lib_manager;
    }

    namespace drivers {
        class graphics_driver;
        class audio_driver;
        class sensor_driver;
    }

    namespace arm {
        class core;
        using core_instance = std::unique_ptr<core>;
    }

    namespace loader {
        struct rom;
    }

    namespace config {
        class app_settings;
        struct app_setting;

        struct state;
    }

    namespace dispatch {
        struct dispatcher;
    }

    namespace manager {
        class packages;
        class scripts;
    }

    namespace j2me {
        class app_list;
    }

    class debugger_base;

    class system;
    class system_impl;

    using hal_instance = std::unique_ptr<epoc::hal>;
    using system_reset_callback_type = std::function<void(system *)>;

    enum zip_mount_error {
        zip_mount_error_none,
        zip_mount_error_not_zip,
        zip_mount_error_no_system_folder,
        zip_mount_error_corrupt
    };

    enum ngage_game_card_install_error {
        ngage_game_card_install_success = 0,
        ngage_game_card_no_game_data_folder = 1,
        ngage_game_card_more_than_one_data_folder = 2,
        ngage_game_card_no_game_registeration_info = 3,
        ngage_game_card_registeration_corrupted = 4,
        ngage_game_card_general_error = 5
    };

    struct system_create_components {
        drivers::graphics_driver *graphics_;
        drivers::audio_driver *audio_;

        config::state *conf_;
        config::app_settings *settings_;

        explicit system_create_components();
    };

    class system {
        system_impl *impl;

    public:
        system(const system &) = delete;
        system &operator=(const system &) = delete;

        system(system &&) = delete;
        system &operator=(system &&) = delete;

        explicit system(system_create_components &param);
        ~system();

        void set_graphics_driver(drivers::graphics_driver *driver);
        void set_audio_driver(drivers::audio_driver *driver);
        void set_sensor_driver(drivers::sensor_driver *driver);
        void set_debugger(debugger_base *new_debugger);
        void set_symbian_version_use(const epocver ever);
        void set_cpu_executor_type(const arm_emulator_type type);

        const arm_emulator_type get_cpu_executor_type() const;

        loader::rom *get_rom_info();
        epocver get_symbian_version_use() const;
        bool is_s80_device_active();

        void prepare_reschedule();

        void startup();
        bool load(const std::u16string &path, const std::u16string &cmd_arg);

        int loop();

        void do_state(common::chunkyseri &seri);

        device_manager *get_device_manager();
        manager::packages *get_packages();
        manager::scripts *get_scripts();
        memory_system *get_memory_system();
        kernel_system *get_kernel_system();
        hle::lib_manager *get_lib_manager();
        io_system *get_io_system();
        ntimer *get_ntimer();
        disasm *get_disasm();
        gdbstub *get_gdb_stub();
        drivers::graphics_driver *get_graphics_driver();
        drivers::audio_driver *get_audio_driver();
        drivers::sensor_driver *get_sensor_driver();
        arm::core *get_cpu();
        config::state *get_config();
        dispatch::dispatcher *get_dispatcher();
        j2me::app_list *get_j2me_applist();

        void set_config(config::state *conf);

        void mount(drive_number drv, const drive_media media, std::string path, const std::uint32_t attrib = io_attrib_none);
        zip_mount_error mount_game_zip(drive_number drv, const drive_media media, const std::string &zip_path, const std::uint32_t base_attrib = io_attrib_none,
            progress_changed_callback progress_cb = nullptr, cancel_requested_callback cancel_cb = nullptr);

        ngage_game_card_install_error install_ngage_game_card(const std::string &folder_path, std::function<void(std::string)> game_name_found_cb, progress_changed_callback progress_cb = nullptr);
        bool get_ngage_game_info_mounted(apa_app_registry &result);

        bool reset();

        bool pause();
        bool unpause();

        bool set_device(const std::uint8_t idx);
        int install_package(std::u16string path, drive_number drv);

        void request_exit();
        bool should_exit() const;

        void add_new_hal(uint32_t hal_category, hal_instance &hal_com);
        epoc::hal *get_hal(uint32_t category);

        const language get_system_language() const;
        void set_system_language(const language new_lang);

        void validate_current_device();
        bool rescan_devices(const drive_number drvrom);

        std::size_t add_system_reset_callback(system_reset_callback_type type);
        bool remove_system_reset_callback(const std::size_t h);

        void initialize_user_parties();

        /**
         * @brief Set the launch app setting.
         *
         * For the usage of launch app setting, refer to the function @ref get_launch_app_setting.<br><br>
         *
         * This is an interface exposed so that frontend can set launch app setting before launching the app.
         * Do not use it if you intend to change some static settings of the app after it is launched.
         *
         * @param setting The setting of the app to launch
         */
        void set_launch_app_setting(const config::app_setting &setting);

        /**
         * @brief Get the launch app setting.
         *
         * Each app has its own setting, when launches through emulator file, the setting of the app
         * can be get through this function.<br><br>
         *
         * Static setting likes this is used for situation such as native resolution mode (when the screen size
         * is changed to native resolution at startup and only once)
         *
         * @return The launch app setting.
         */
        const config::app_setting &get_launch_app_setting();
    };
}
