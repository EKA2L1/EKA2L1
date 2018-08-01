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

#include <core/arm/jit_factory.h>
#include <core/core_kernel.h>
#include <core/core_mem.h>
#include <core/core_timing.h>
#include <core/disasm/disasm.h>
#include <core/hle/libmanager.h>
#include <core/loader/rom.h>
#include <core/manager/manager.h>
#include <core/manager/package_manager.h>
#include <core/vfs.h>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <process.h>
#include <tuple>

#include <core/drivers/emu_window.h>
#include <core/drivers/screen_driver.h>

namespace eka2l1 {
    namespace epoc {
        struct hal;
    }

    using hal_ptr = std::shared_ptr<epoc::hal>;

    enum class availdrive {
        c,
        e,
        z
    };

    /*! A system instance, where all the magic happens. 
     *
     * Represents the Symbian system. You can switch the system version dynamiclly.
    */
    class system {
        //! Global lock mutex for system.
        std::mutex mut;

        //! The library manager.
        hle::lib_manager hlelibmngr;

        //! The cpu
        arm::jitter cpu;

        //! Jit type.
        arm::jitter_arm_type jit_type;

        //! The window type to use.
        driver::window_type win_type;

        //! The driver type to use (GL, Vulkan, ...)
        driver::driver_type dr_type;

        //! The memory system.
        memory_system mem;

        //! The kernel system.
        kernel_system kern;

        //! The timing system.
        timing_system timing;

        //! Manager system.
        /*!
            This manages all apps and dlls.
        */
        manager_system mngr;

        //! The IO system
        io_system io;

        //! Disassmebly helper.
        disasm asmdis;

        //! The ROM
        /*! This is the information parsed
         * from the ROM, used as utility.
        */
        loader::rom romf;

        bool reschedule_pending;

        epocver ver = epocver::epoc9;
        bool exit = false;

        driver::emu_window_ptr emu_win;
        driver::screen_driver_ptr emu_screen_driver;

        std::unordered_map<std::string, bool> bool_configs;
        std::unordered_map<uint32_t, hal_ptr> hals;

        std::vector<std::string> startup_apps;

        bool startup_inited = false;

        /*! \brief Load the core configs.
        */
        void load_configs();

        /*! \brief Save the core configs. */
        void write_configs();

    public:
        bool get_bool_config(const std::string name) {
            return bool_configs[name];
        }

        system(driver::window_type emu_win_type = driver::window_type::glfw,
            driver::driver_type emu_driver_type = driver::driver_type::opengl,
            arm::jitter_arm_type jit_type = arm::jitter_arm_type::unicorn)
            : jit_type(jit_type)
            , win_type(emu_win_type)
            , dr_type(emu_driver_type) {}

        void set_symbian_version_use(const epocver ever) {
            kern.set_epoc_version(ever);
            io.set_epoc_version(ever);
        }

        loader::rom &get_rom_info() {
            return romf;
        }

        epocver get_symbian_version_use() const {
            return kern.get_epoc_version();
        }

        void prepare_reschedule() {
            cpu->prepare_rescheduling();
            reschedule_pending = true;
        }

        void init();
        uint32_t load(uint32_t id);
        int loop();
        void shutdown();

        memory_system *get_memory_system() {
            return &mem;
        }

        kernel_system *get_kernel_system() {
            return &kern;
        }

        hle::lib_manager *get_lib_manager() {
            return &hlelibmngr;
        }

        io_system *get_io_system() {
            return &io;
        }

        timing_system *get_timing_system() {
            return &timing;
        }

        disasm *get_disasm() {
            return &asmdis;
        }

        driver::emu_window_ptr get_emu_window() {
            return emu_win;
        }

        driver::screen_driver_ptr get_screen_driver() {
            return emu_screen_driver;
        }

        arm::jitter &get_cpu() {
            return cpu;
        }

        void mount(availdrive drv, std::string path, bool in_mem = false);
        void reset();

        bool install_rpkg(const std::string &path);

        bool install_package(std::u16string path, uint8_t drv);
        bool load_rom(const std::string &path);

        void request_exit();
        bool should_exit() const {
            return exit;
        }

        size_t total_app() {
            return mngr.get_package_manager()->app_count();
        }

        std::vector<manager::app_info> app_infos() {
            return mngr.get_package_manager()->get_apps_info();
        }

        void add_new_hal(uint32_t hal_cagetory, hal_ptr hal_com);
        hal_ptr get_hal(uint32_t cagetory);
    };
}
