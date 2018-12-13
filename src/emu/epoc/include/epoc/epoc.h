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
        class graphics_driver_client;
        class driver;

        using driver_instance = std::shared_ptr<driver>;
    }

    namespace arm {
        class arm_interface;
        using jitter = std::unique_ptr<arm_interface>;
    }

    class debugger_base;
    using debugger_ptr = std::shared_ptr<debugger_base>;

    using hal_ptr = std::shared_ptr<epoc::hal>;
    using graphics_driver_client_ptr = std::shared_ptr<drivers::graphics_driver_client>;

    namespace loader {
        struct rom;
    }

    class system_impl;

    /*! A system instance, where all the magic happens. 
     *
     * Represents the Symbian system. You can switch the system version dynamiclly.
    */
    class system {
        // TODO: Make unique
        std::shared_ptr<system_impl> impl;

    public:    
        system(const system&) = delete;
        system& operator=(const system&) = delete;

        system(system&&) = delete;
        system& operator=(system&&) = delete;

        system(debugger_ptr debugger, drivers::driver_instance graphics_driver,
            arm_emulator_type jit_type = arm_emulator_type::unicorn);

        ~system() = default;

        bool get_bool_config(const std::string name);
        void set_graphics_driver(drivers::driver_instance graphics_driver);
        void set_debugger(debugger_ptr new_debugger);
        void set_symbian_version_use(const epocver ever);
        void set_jit_type(const arm_emulator_type type);

        loader::rom *get_rom_info();
        epocver get_symbian_version_use() const;

        void prepare_reschedule();

        void init();
        uint32_t load(uint32_t id);
        
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
        graphics_driver_client_ptr get_graphic_driver_client();
        arm::jitter &get_cpu();

        void mount(drive_number drv, const drive_media media, std::string path,
            const io_attrib attrib = io_attrib::none);

        void reset();

        bool install_rpkg(const std::string &path);
        void load_scripts();

        /*! \brief Install a SIS/SISX. */
        bool install_package(std::u16string path, uint8_t drv);
        bool load_rom(const std::string &path);

        void request_exit();
        bool should_exit() const;

        void add_new_hal(uint32_t hal_cagetory, hal_ptr hal_com);
        hal_ptr get_hal(uint32_t cagetory);
    };
}
