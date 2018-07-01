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
#include <core.h>
#include <process.h>

#include <common/log.h>
#include <common/cvt.h>
#include <disasm/disasm.h>

#include <loader/eka2img.h>
#include <loader/rpkg.h>

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
        mem.init();

        io.init(&mem);
        mngr.init(&io);
        asmdis.init(&mem);

        cpu = arm::create_jitter(&timing, &mem, &asmdis, &hlelibmngr, jit_type);
        emu_win = driver::new_emu_window(win_type);
        emu_screen_driver = driver::new_screen_driver(dr_type);

        kern.init(this, &timing, &mngr, &mem, &io, &hlelibmngr, cpu.get());
    }

    process *system::load(uint32_t id) {
        emu_win->init("EKA2L1", vec2(360, 640));
        emu_screen_driver->init(emu_win, object_size(360, 640), object_size(15, 15));

        emu_win->close_hook = [&]() {
            exit = true;
        };

        crr_process = kern.spawn_new_process(id);

        if (crr_process == nullptr) {
            return nullptr;
        }

        crr_process->run();

        emu_win->change_title("EKA2L1 | " + 
            common::ucs2_to_utf8(mngr.get_package_manager()->app_name(id)) + " (" + 
            common::to_string(id, std::hex) + ")");

        return crr_process;
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

        if (!exit) {
            kern.reschedule();
            kern.processing_requests();
            reschedule_pending = false;
        } else {
            if (kern.crr_process()) {
                kern.close_process(kern.crr_process());
			}
		}

        if (kern.get_thread_scheduler()->should_terminate()) {
            emu_screen_driver->shutdown();
            emu_win->shutdown();

			kern.close_process(kern.crr_process());

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

        bool res1 = mem.load_rom(romf.header.rom_base, path);

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
        io.mount(((drv == availdrive::c) ? "C:" : ((drv == availdrive::z) ? "Z:": "E:")), path, in_mem);
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

        emitter << YAML::EndMap;

        std::ofstream out("coreconfig.yml");
        out << emitter.c_str();
    }

    void system::load_configs() {
        try {
            YAML::Node node = YAML::LoadFile("coreconfig.yml");

            for (auto const &subnode : node) {
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
}

