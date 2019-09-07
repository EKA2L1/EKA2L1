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
    namespace common {
        class chunkyseri;
    }

    namespace epoc {
        struct hal;
    }

    class memory_system;
    class manager_system;
    class kernel_system;

    namespace hle {
        class lib_manager;
    }

    class io_system;
    class timing_system;
    class disasm;
    class gdbstub;

    namespace drivers {
        class graphics_driver;
    }

    namespace arm {
        class arm_interface;
        using jitter = std::unique_ptr<arm_interface>;
    }

    class debugger_base;
    using debugger_ptr = std::shared_ptr<debugger_base>;

    using hal_instance = std::unique_ptr<epoc::hal>;

    namespace loader {
        struct rom;
    }

    namespace manager {
        struct config_state;
    }

    class system_impl;

    /*! A system instance, where all the magic happens. 
     *
     * Represents the Symbian system. You can switch the system version dynamiclly.
    */
    class system {
        // TODO: Make unique
        std::unique_ptr<system_impl> impl;

    public:
        system(const system &) = delete;
        system &operator=(const system &) = delete;

        system(system &&) = delete;
        system &operator=(system &&) = delete;

        system(debugger_ptr debugger, drivers::graphics_driver *graphics_driver,
            manager::config_state *conf);

        ~system() = default;

        void set_graphics_driver(drivers::graphics_driver *driver);
        void set_debugger(debugger_ptr new_debugger);
        void set_symbian_version_use(const epocver ever);
        void set_jit_type(const arm_emulator_type type);

        loader::rom *get_rom_info();
        epocver get_symbian_version_use() const;

        void prepare_reschedule();

        void init();
        bool load(const std::u16string &path, const std::u16string &cmd_arg);

        int loop();
        void shutdown();

        void do_state(common::chunkyseri &seri);

        manager_system *get_manager_system();
        memory_system *get_memory_system();
        kernel_system *get_kernel_system();
        hle::lib_manager *get_lib_manager();
        io_system *get_io_system();
        timing_system *get_timing_system();
        disasm *get_disasm();
        gdbstub *get_gdb_stub();
        drivers::graphics_driver *get_graphics_driver();
        arm::jitter &get_cpu();
        manager::config_state *get_config();

        void set_config(manager::config_state *conf);

        void mount(drive_number drv, const drive_media media, std::string path,
            const io_attrib attrib = io_attrib::none);

        void reset();

        bool install_rpkg(const std::string &devices_rom_path, const std::string &path);
        void load_scripts();

        /*! \brief Set the current device that the system will emulate.
        */
        bool set_device(const std::uint8_t idx);

        /*! \brief Install a SIS/SISX. */
        bool install_package(std::u16string path, drive_number drv);
        bool load_rom(const std::string &path);

        void request_exit();
        bool should_exit() const;

        void add_new_hal(uint32_t hal_cagetory, hal_instance &hal_com);
        epoc::hal *get_hal(uint32_t cagetory);

        const language get_system_language() const;
        void set_system_language(const language new_lang);
    };
}
