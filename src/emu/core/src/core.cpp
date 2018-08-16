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
#include <core/core.h>
#include <core/kernel/process.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <core/disasm/disasm.h>

#include <core/loader/eka2img.h>
#include <core/loader/rpkg.h>

#include <core/epoc/hal.h>

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace eka2l1 {
    void system::init() {
        exit = false;

        if (!already_setup)
            log::setup_log(nullptr);

        load_configs();

        // Initialize all the system that doesn't depend on others first
        timing.init();

        io.init(&mem, get_symbian_version_use());
        mngr.init(this, &io);
        asmdis.init(&mem);

        cpu = arm::create_jitter(&timing, &mem, &asmdis, &hlelibmngr, jit_type);

        mem.init(cpu, get_symbian_version_use() <= epocver::epoc6 ? ram_code_addr_eka1 : ram_code_addr,
            get_symbian_version_use() <= epocver::epoc6 ? shared_data_eka1 : shared_data,
            get_symbian_version_use() <= epocver::epoc6 ? shared_data_end_eka1 - shared_data_eka1 : ram_code_addr - shared_data);

        emu_win = driver::new_emu_window(win_type);
        emu_screen_driver = driver::new_screen_driver(dr_type);

        kern.init(this, &timing, &mngr, &mem, &io, &hlelibmngr, cpu.get());
        epoc::init_hal(this);

        bool res = mngr.get_script_manager()->import_module("scripts/hello_panic");
    }

    uint32_t system::load(uint32_t id) {
        emu_win->init("EKA2L1", vec2(360, 640));
        emu_screen_driver->init(emu_win, object_size(360, 640), object_size(15, 15));

        emu_win->close_hook = [&]() {
            exit = true;
        };

        uint32_t process_handle = kern.spawn_new_process(id);

        if (process_handle == INVALID_HANDLE) {
            return INVALID_HANDLE;
        }

        kern.run_process(process_handle);

        // Change window title to game title
        emu_win->change_title("EKA2L1 | " + common::ucs2_to_utf8(mngr.get_package_manager()->app_name(id)) + " (" + common::to_string(id, std::hex) + ")");

        if (!startup_inited) {
            for (auto &startup_app : startup_apps) {
                uint32_t process = kern.spawn_new_process(startup_app, eka2l1::filename(startup_app), 0x123456);
                kern.run_process(process);
            }

            startup_inited = true;
        }

        return process_handle;
    }

    int system::loop() {
        if (kern.crr_thread() == nullptr) {
            timing.idle();
            timing.advance();
            prepare_reschedule();
        } else {
            timing.advance();
            cpu->run();
        }

        if (!kern.should_terminate()) {
            kern.processing_requests();
            kern.reschedule();

            reschedule_pending = false;
        } else {
            emu_screen_driver->shutdown();
            emu_win->shutdown();

            kern.crr_process().reset();

            exit = true;
            return 0;
        }

        return 1;
    }

    bool system::install_package(std::u16string path, uint8_t drv) {
        return mngr.get_package_manager()->install_package(path, drv);
    }

    bool system::load_rom(const std::string &path) {
        auto romf_res = loader::load_rom(path);

        if (!romf_res) {
            return false;
        }

        romf = *romf_res;

        bool res1 = mem.map_rom(romf.header.rom_base, path);

        if (!res1) {
            return false;
        }

        return true;
    }

    void system::shutdown() {
        timing.shutdown();
        kern.shutdown();
        hlelibmngr.shutdown();
        mem.shutdown();
        asmdis.shutdown();

        exit = false;
    }

    void system::mount(availdrive drv, std::string path, bool in_mem) {
        io.mount(((drv == availdrive::c) ? "C:" : ((drv == availdrive::z) ? "Z:" : "E:")), path, in_mem);
    }

    void system::request_exit() {
        cpu->stop();
        exit = true;
    }

    void system::reset() {
        exit = false;
        emu_screen_driver->reset();
        hlelibmngr.reset();
    }

    bool system::install_rpkg(const std::string &path) {
        std::atomic_int holder;
        return loader::install_rpkg(&io, path, holder);
    }

    void system::write_configs() {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        for (auto & [ name, op ] : bool_configs) {
            emitter << YAML::Key << name << YAML::Value << op;
        }

        emitter << YAML::Key << "startup" << YAML::Value << YAML::BeginDoc;

        for (const auto &app : startup_apps) {
            emitter << app;
        }

        emitter << YAML::EndDoc;

        emitter << YAML::EndMap;

        std::ofstream out("coreconfig.yml");
        out << emitter.c_str();
    }

    void system::load_configs() {
        try {
            YAML::Node node = YAML::LoadFile("coreconfig.yml");

            for (auto const &subnode : node) {
                if (subnode.first.as<std::string>() == "startup") {
                    for (const auto &startup_app : subnode.second) {
                        startup_apps.push_back(startup_app.as<std::string>());
                    }

                    continue;
                }

                bool_configs.emplace(subnode.first.as<std::string>(), subnode.second.as<bool>());
            }

        } catch (...) {
            LOG_WARN("Loading CORE config incompleted due to an exception. Use default");

            bool_configs.emplace("log_code", false);
            bool_configs.emplace("log_passed", false);
            bool_configs.emplace("log_write", false);
            bool_configs.emplace("log_read", false);
            bool_configs.emplace("log_exports", false);

            write_configs();
        }
    }

    void system::add_new_hal(uint32_t hal_cagetory, hal_ptr hal_com) {
        hals.emplace(hal_cagetory, std::move(hal_com));
    }

    hal_ptr system::get_hal(uint32_t cagetory) {
        return hals[cagetory];
    }
}
