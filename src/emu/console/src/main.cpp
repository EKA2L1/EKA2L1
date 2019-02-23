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

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <common/arghandler.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/types.h>
#include <console/cmdhandler.h>
#include <console/guithread.h>
#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <drivers/graphics/emu_window.h>
#include <epoc/epoc.h>
#include <epoc/loader/rom.h>

#include <imgui.h>
#include <yaml-cpp/yaml.h>
#include <gdbstub/gdbstub.h>

using namespace eka2l1;

std::unique_ptr<eka2l1::system> symsys = std::make_unique<eka2l1::system>(nullptr, nullptr);
arm_emulator_type jit_type = decltype(jit_type)::unicorn;

std::string rom_path = "SYM.ROM";
std::string mount_c = "drives/c/";
std::string mount_e = "drives/e/";
std::string mount_z = "drives/z/";

std::uint16_t gdb_port = 24689;
bool enable_gdbstub = false;

YAML::Node config;

std::atomic<bool> should_quit(false);
std::atomic<bool> should_pause(false);

bool debugger_quit = false;

std::mutex ui_debugger_mutex;
ImGuiContext *ui_debugger_context;

bool ui_window_mouse_down[5] = { false, false, false, false, false };

std::shared_ptr<eka2l1::imgui_logger> logger;
std::shared_ptr<eka2l1::imgui_debugger> debugger;

std::mutex lock;
std::condition_variable cond;

std::uint8_t device_to_use = 0;         ///< Device that will be used

void read_config() {
    try {
        config = YAML::LoadFile("config.yml");

        rom_path = config["rom_path"].as<std::string>();
        
        // TODO: Expand more drives
        mount_c = config["c_mount"].as<std::string>();
        mount_e = config["e_mount"].as<std::string>();

        device_to_use = config["device"].as<int>();

        const std::string jit_type_raw = config["jitter"].as<std::string>();

        if (jit_type_raw == "dynarmic") {
            jit_type = decltype(jit_type)::dynarmic;
        }
        
        enable_gdbstub = config["enable_gdbstub"].as<bool>();
        gdb_port = config["gdb_port"].as<int>();
    } catch (...) {
        return;
    }
}

void save_config() {
    config["rom_path"] = rom_path;
    config["c_mount"] = mount_c;
    config["e_mount"] = mount_e;
    config["device"] = static_cast<int>(device_to_use);
    config["enable_gdbstub"] = enable_gdbstub;

    std::ofstream config_file("config.yml");
    config_file << config;
}

void init() {
    symsys->set_jit_type(jit_type);
    symsys->set_debugger(debugger);

    symsys->init();

    symsys->set_device(device_to_use);
    symsys->mount(drive_c, drive_media::physical, mount_c, io_attrib::internal);
    symsys->mount(drive_e, drive_media::physical, mount_e, io_attrib::removeable);

    if (enable_gdbstub) {
        symsys->get_gdb_stub()->set_server_port(gdb_port);
        symsys->get_gdb_stub()->init(symsys.get());
        symsys->get_gdb_stub()->toggle_server(true);
    }

    bool res = symsys->load_rom(rom_path);

    // Mount the drive Z after the ROM was loaded. The ROM load than a new FS will be
    // created for ROM purpose.
    symsys->mount(drive_z, drive_media::rom,
        mount_z, io_attrib::internal | io_attrib::write_protected);
}

void shutdown() {
    symsys->shutdown();
}

void do_quit() {
    save_config();
    symsys->shutdown();
}

void run() {
    while (!should_quit && !symsys->should_exit()) {
        symsys->loop();

        if (should_pause && !should_quit) {
            debugger->wait_for_debugger();
        }
    }
}

int main(int argc, char **argv) {
    // Episode 1: I think of a funny joke about Symbian and I will told my virtual wife
    std::cout << "-------------- EKA2L1: Experimental Symbian Emulator -----------------" 
        << std::endl;
   
    // We are going to setup to GUI logger (in debugger)
    logger = std::make_shared<eka2l1::imgui_logger>();
    log::setup_log(logger);

    // Start to read the configs
    read_config();

    if (should_quit) {
        do_quit();
        return 0;
    }

    eka2l1::drivers::init_window_library(eka2l1::drivers::window_type::glfw);
    debugger = std::make_shared<eka2l1::imgui_debugger>(symsys.get(), logger);

    init();
    
    // Let's set up this
    eka2l1::common::arg_parser parser(argc, argv);
    
    parser.add("--irpkg", "Install a repackage file. The installer will auto recognizes "
        "the product and system info", rpkg_unpack_option_handler);
    parser.add("--help, --h", "Display helps menu", help_option_handler);
    parser.add("--listapp", "List all installed applications", list_app_option_handler);
    parser.add("--listdevices", "List all installed devices", list_devices_option_handler);
    parser.add("--app, --a, --run", "Run an app with specified index. See index of an app in --listapp",
        app_specifier_option_handler);
    parser.add("--install, --i", "Install a SIS", app_install_option_handler);

    if (argc > 1) {
        std::string err;
        should_quit = !parser.parse(&err);

        if (should_quit) {
            std::cout << err << std::endl;
        }
    }

    if (should_quit) {
        do_quit();
        return 0;
    }

    std::thread debug_window_thread(ui_debugger_thread);
    std::unique_lock<std::mutex> ulock(lock);

    // Wait for debug thread to intialize
    cond.wait(ulock);

    run();

    debug_window_thread.join();
    do_quit();

    // Kill the system
    symsys.reset();

    eka2l1::drivers::destroy_window_library(eka2l1::drivers::window_type::glfw);

    return 0;
}