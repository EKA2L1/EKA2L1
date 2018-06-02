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
#include <disasm/disasm.h>
#include <loader/eka2img.h>

namespace eka2l1 {
    void system::init() {
        log::setup_log(nullptr);

        // Initialize all the system that doesn't depend on others first
        timing.init();
        mem.init();

        io.init(&mem);
        mngr.init(&io);
        asmdis.init(&mem);

        // Lib manager needs the system to call HLE function
        hlelibmngr.init(this, &kern, &io, &mem, ver);

        cpu = arm::create_jitter(&timing, &mem, &asmdis, &hlelibmngr, jit_type);

        kern.init(&timing, &mngr, &mem, &io, &hlelibmngr, cpu.get());
    }

    process *system::load(uint64_t id) {
        crr_process = kern.spawn_new_process(id);
        crr_process->run();
        return crr_process;
    }

    int system::loop() {
        auto prepare_reschedule = [&]() {
            cpu->prepare_rescheduling();
            reschedule_pending = true;
        };

        if (kern.crr_thread() == nullptr) {
            timing.idle();
            timing.advance();
            prepare_reschedule();
        } else {
            timing.advance();
            cpu->run();
        }

        kern.reschedule();
        reschedule_pending = false;

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

        romf = romf_res.value();
        io.mount_rom("Z:", &romf);

        bool res1 = mem.load_rom(path);

        if (!res1) {
            return false;
        }

        return true;
    }

    void system::shutdown() {
        timing.shutdown();
        kern.shutdown();
        mem.shutdown();
        asmdis.shutdown();
    }

    void system::mount(availdrive drv, std::string path) {
        io.mount(((drv == availdrive::c) ? "C:" : "E:"), path);
    }
}

