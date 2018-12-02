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
#include <core/configure.h>
#include <core/kernel/process.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/random.h>

#include <core/disasm/disasm.h>

#include <core/loader/eka2img.h>
#include <core/loader/rpkg.h>

#include <core/epoc/hal.h>
#include <core/epoc/panic.h>

#ifdef ENABLE_SCRIPTING
#include <core/manager/script_manager.h>
#endif

#include <yaml-cpp/yaml.h>

#include <experimental/filesystem>

#include <atomic>
#include <fstream>
#include <string>

#include <drivers/itc.h>

namespace fs = std::experimental::filesystem;

namespace eka2l1 {
    void system::load_scripts() {
#ifdef ENABLE_SCRIPTING
        for (const auto &entry : fs::directory_iterator("scripts")) {
            if (fs::is_regular_file(entry.path()) && entry.path().extension() == ".py") {
                auto module_name = entry.path().filename().replace_extension("").string();
                mngr.get_script_manager()->import_module("scripts/" + module_name);
            }
        }
#endif
    }

    void system::init() {
        exit = false;
        load_configs();

        // Initialize all the system that doesn't depend on others first
        timing.init();

        io.init();
        mngr.init(this, &io);
        asmdis.init(&mem);

        file_system_inst physical_fs = create_physical_filesystem(get_symbian_version_use());
        io.add_filesystem(physical_fs);

        file_system_inst rom_fs = create_rom_filesystem(nullptr, &mem, 
            get_symbian_version_use());

        rom_fs_id = io.add_filesystem(rom_fs);

        cpu = arm::create_jitter(&kern, &timing, &mngr, &mem, &asmdis, &hlelibmngr, &gdb_stub, debugger, jit_type);

        mem.init(cpu, get_symbian_version_use() <= epocver::epoc6 ? ram_code_addr_eka1 : ram_code_addr,
            get_symbian_version_use() <= epocver::epoc6 ? shared_data_eka1 : shared_data,
            get_symbian_version_use() <= epocver::epoc6 ? shared_data_end_eka1 - shared_data_eka1 : ram_code_addr - shared_data);

        kern.init(this, &timing, &mngr, &mem, &io, &hlelibmngr, cpu.get());

        epoc::init_hal(this);
        epoc::init_panic_descriptions();

#if ENABLE_SCRIPTING == 1
        load_scripts();
#endif
    }

    system::system(debugger_ptr debugger, drivers::driver_instance graphics_driver,
        arm::jitter_arm_type jit_type)
        : jit_type(jit_type) {
        gdriver_client = std::make_shared<drivers::graphics_driver_client>(graphics_driver);
    }

    void system::set_graphics_driver(drivers::driver_instance graphics_driver) {
        gdriver_client = std::make_shared<drivers::graphics_driver_client>(graphics_driver);
    }

    uint32_t system::load(uint32_t id) {
        hlelibmngr.reset();
        hlelibmngr.init(this, &kern, &io, &mem, get_symbian_version_use());

        for (const auto &force_load_lib : force_load_libs) {
            loader::romimg_ptr img = hlelibmngr.load_romimg(common::utf8_to_ucs2(force_load_lib), false);

            if (img) {
                hlelibmngr.open_romimg(img);
            }
        }

        if (!startup_inited) {
            for (auto &startup_app : startup_apps) {
                uint32_t process = kern.spawn_new_process(startup_app, eka2l1::filename(startup_app));
                
                kern.run_process(process);
            }

            startup_inited = true;
        }

        uint32_t process_handle = kern.spawn_new_process(id);

        if (process_handle == INVALID_HANDLE) {
            return INVALID_HANDLE;
        }

        kern.run_process(process_handle);
        return process_handle;
    }

    int system::loop() {
        bool should_step = false;

        if (gdb_stub.is_server_enabled()) {
            gdb_stub.handle_packet();

            if (gdb_stub.get_cpu_halt_flag()) {
                if (gdb_stub.get_cpu_step_flag()) {
                    should_step = true;
                } else {
                    return 1;
                }
            }
        }

        if (kern.crr_thread() == nullptr) {
            timing.idle();
            timing.advance();
            prepare_reschedule();
        } else {
            timing.advance();

            if (!should_step) {
                cpu->run();
            } else {
                cpu->step();
            }
        }

        if (!kern.should_terminate()) {
            kern.processing_requests();

#ifdef ENABLE_SCRIPTING
            mngr.get_script_manager()->call_reschedules();
#endif
            
            kern.reschedule();

            reschedule_pending = false;
        } else {
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
        std::optional<loader::rom> romf_res = loader::load_rom(path);

        if (!romf_res) {
            return false;
        }

        romf = std::move(*romf_res);

        if (rom_fs_id) {
            io.remove_filesystem(*rom_fs_id);
        }

        file_system_inst rom_fs = create_rom_filesystem(&romf, &mem, 
            get_symbian_version_use());

        rom_fs_id = io.add_filesystem(rom_fs);

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

    void system::mount(drive_number drv, const drive_media media, std::string path, 
        const io_attrib attrib) {
        io.mount_physical_path(drv, media, attrib, common::utf8_to_ucs2(path));
    }

    void system::request_exit() {
        cpu->stop();
        exit = true;
    }

    void system::reset() {
        exit = false;
        hlelibmngr.reset();
    }

    bool system::install_rpkg(const std::string &path) {
        std::atomic_int holder;
        bool res = eka2l1::loader::install_rpkg(&io, path, holder);

        if (!res) {
            return false;
        }

        return true;
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
                } else if (subnode.first.as<std::string>() == "force_load") {
                    for (const auto &startup_app : subnode.second) {
                        force_load_libs.push_back(startup_app.as<std::string>());
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
            bool_configs.emplace("log_svc_passed", false);
            bool_configs.emplace("enable_breakpoint_script", false);
            bool_configs.emplace("log_exports", false);
            bool_configs.emplace("log_ipc", false);

            write_configs();
        }
    }

    void system::add_new_hal(uint32_t hal_cagetory, hal_ptr hal_com) {
        hals.emplace(hal_cagetory, std::move(hal_com));
    }

    hal_ptr system::get_hal(uint32_t cagetory) {
        return hals[cagetory];
    }

    bool system::save_snapshot_processes(const std::string &path,
        const std::vector<uint32_t> &include_uids) {
        page_table *pt = mem.get_current_page_table();

        // Can't snapshot if no memory page table is present
        if (!pt) {
            return false;
        }

        FILE *f = fopen(path.data(), "wb");

        // Start writing the magic header
        const char *magic_header = "SNAE";
        fwrite(magic_header, 1, 4, f);

        // Kernel object saving
        kern.save_snapshot_for_processes(f, include_uids);
        
        return true;
    }
}