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

#include <common/cvt.h>
#include <common/log.h>
#include <common/types.h>

#include <epoc/epoc.h>
#include <epoc/configure.h>
#include <epoc/loader/rom.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>

#include <imgui.h>
#include <yaml-cpp/yaml.h>

#include <manager/manager.h>
#include <gdbstub/gdbstub.h>

#include <common/ini.h>

using namespace eka2l1;

std::unique_ptr<eka2l1::system> symsys = std::make_unique<eka2l1::system>(nullptr, nullptr);

arm_emulator_type jit_type = decltype(jit_type)::unicorn;
epocver ever = epocver::epoc9;

std::string rom_path = "SYM.ROM";
std::string mount_c = "drives/c/";
std::string mount_e = "drives/e/";
std::string mount_z = "drives/z/";
std::string sis_install_path = "-1";

std::string rpkg_path;
std::uint16_t gdb_port = 24689;
std::uint8_t adrive;

bool enable_gdbstub = false;

YAML::Node config;

int drive_mount;
int app_idx = -1;

bool help_printed = false;
bool list_app = false;
bool install_rpkg = false;

std::atomic<bool> should_quit(false);
std::atomic<bool> should_pause(false);

bool debugger_quit = false;

std::mutex ui_debugger_mutex;

ImGuiContext *ui_debugger_context;

bool ui_window_mouse_down[5] = { false, false, false, false, false };

std::shared_ptr<eka2l1::imgui_logger> logger;
std::shared_ptr<eka2l1::drivers::graphics_driver> gdriver;
std::shared_ptr<eka2l1::imgui_debugger> debugger;
std::shared_ptr<eka2l1::drivers::emu_window> debugger_window;

std::mutex lock;
std::condition_variable cond;

void print_help() {
    std::cout << "Usage: Drag and drop Symbian file here, ignore missing dependencies" << std::endl;
    std::cout << "Options: " << std::endl;
    std::cout << "\t -rom: Specified where the ROM is located. If none is specified, the emu will look for a file named SYM.ROM." << std::endl;
    std::cout << "\t -ver: Specified Symbian version to emulate (either 6 or 9)." << std::endl;
    std::cout << "\t -app: Specified the app to run. Next to this option is the index number." << std::endl;
    std::cout << "\t -listapp: List all of the apps." << std::endl;
    std::cout << "\t -install: Install a SIS/SISX package" << std::endl;
    std::cout << "\t -irpkg ver path: Install RPKG." << std::endl;
    std::cout << "\t\t ver:  Epoc version. Available version are: v94, v93, belle, v60" << std::endl;
    std::cout << "\t\t path: Path to RPKG file." << std::endl;
    std::cout << "\t -h/-help: Print help" << std::endl;
}

void fetch_rpkg(const char *ver, const char *path) {
    install_rpkg = true;

    rpkg_path = path;
    std::string ver_str = ver;

    if (ver_str == "v93") {
        ever = epocver::epoc93;
    } else if (ver_str == "v94") {
        ever = epocver::epoc9;
    } else if (ver_str == "belle") {
        ever = epocver::epoc10;
    } else if (ver_str == "v60") {
        ever = epocver::epoc6;
    }
}

void parse_args(int argc, char **argv) {
    if (argc <= 1) {
        print_help();
        should_quit = true;
        return;
    }

    for (int i = 1; i <= argc - 1; i++) {
        if (strncmp(argv[i], "-rom", 4) == 0) {
            rom_path = argv[++i];
            config["rom_path"] = rom_path;
        } else if (((strncmp(argv[i], "-h", 2)) == 0 || (strncmp(argv[i], "-help", 5) == 0))
            && (!help_printed)) {
            print_help();
            help_printed = true;
            should_quit = true;
        } else if ((strncmp(argv[i], "-ver", 4) == 0 || (strncmp(argv[i], "-v", 2) == 0))) {
            int ver = std::atoi(argv[++i]);

            if (ver == 6) {
                ever = epocver::epoc6;
                config["epoc_ver"] = (int)ever;
            } else {
                ever = epocver::epoc9;
                config["epoc_ver"] = (int)ever;
            }
        } else if (strncmp(argv[i], "-app", 4) == 0) {
            try {
                app_idx = std::atoi(argv[++i]);
            } catch (...) {
                std::cout << "Invalid request." << std::endl;

                should_quit = true;
                break;
            }
        } else if (strncmp(argv[i], "-listapp", 8) == 0) {
            list_app = true;
        } else if (strncmp(argv[i], "-install", 8) == 0) {
            try {
                adrive = std::atoi(argv[++i]);
            } catch (...) {
                std::cout << "Invalid request." << std::endl;

                should_quit = true;
                break;
            }

            sis_install_path = argv[++i];
        } else if (strncmp(argv[i], "-mount", 6) == 0) {
            drive_mount = std::atoi(argv[++i]);

            if (drive_mount == 0) {
                mount_c = argv[++i];
            } else {
                mount_e = argv[++i];
            }
        } else if (strncmp(argv[i], "-irpkg", 6) == 0) {
            i += 2;

            fetch_rpkg(argv[i - 1], argv[i]);
        } else {
            std::cout << "Invalid request." << std::endl;

            should_quit = true;
            break;
        }
    }
}

void read_config() {
    try {
        config = YAML::LoadFile("config.yml");

        rom_path = config["rom_path"].as<std::string>();
        ever = (epocver)(config["epoc_ver"].as<int>());

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

void do_args() {
    auto infos = symsys->get_manager_system()->get_package_manager()->get_apps_info();

    if (list_app) {
        for (auto &info : infos) {
            std::cout << "[0x" << common::to_string(info.id, std::hex) << "]: "
                      << common::ucs2_to_utf8(info.name) << " (drive: " << ((info.drive == 0) ? 'C' : 'E')
                      << " , executable name: " << common::ucs2_to_utf8(info.executable_name) << ")" << std::endl;
        }

        should_quit = true;
        return;
    }

    if (app_idx > -1) {
        if (app_idx >= infos.size()) {
            LOG_ERROR("Invalid app index.");
            should_quit = true;
            return;
        }

        symsys->load(infos[app_idx].id);
        return;
    }

    if (sis_install_path != "-1") {
        auto res = symsys->install_package(common::utf8_to_ucs2(sis_install_path), 
            adrive == 0 ? drive_c : drive_e);

        if (res) {
            std::cout << "Install successfully!" << std::endl;
        } else {
            std::cout << "Install failed" << std::endl;
        }

        should_quit = true;
    }

    if (install_rpkg) {
        symsys->set_symbian_version_use(ever);
        bool res = symsys->install_rpkg(rpkg_path);

        if (!res) {
            std::cout << "RPKG install failed." << std::endl;
        } else {
            std::cout << "RPKG install successfully." << std::endl;
        }

        should_quit = true;
    }
}

void init() {
    symsys->set_symbian_version_use(ever);
    symsys->set_jit_type(jit_type);
    symsys->set_debugger(debugger);

    symsys->init();
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

void save_config() {
    config["rom_path"] = rom_path;
    config["epoc_ver"] = (int)ever;
    config["c_mount"] = mount_c;
    config["e_mount"] = mount_e;
    config["enable_gdbstub"] = enable_gdbstub;

    std::ofstream config_file("config.yml");
    config_file << config;
}

void do_quit() {
    save_config();
    symsys->shutdown();
}

void set_mouse_down(const int button, const bool op) {
    const std::lock_guard<std::mutex> guard(ui_debugger_mutex);
    ui_window_mouse_down[button] = op;
}

static void on_ui_window_mouse_evt(eka2l1::point mouse_pos, int button, int action) {
    ImGuiIO &io = ImGui::GetIO();
    io.MousePos = ImVec2(static_cast<float>(mouse_pos.x),
        static_cast<float>(mouse_pos.y));

    if (action == 0) {
        set_mouse_down(button, true);
    }
}

static void on_ui_window_mouse_scrolling(eka2l1::vec2 v) {
    ImGuiIO &io = ImGui::GetIO();
    io.MouseWheel += static_cast<float>(v.y);
}

#define KEY_TAB 258
#define KEY_BACKSPACE 259
#define KEY_LEFT_SHIFT 340
#define KEY_LEFT_CONTROL 341
#define KEY_LEFT_ALT 342
#define KEY_LEFT_SUPER 343
#define KEY_RIGHT_SHIFT 344
#define KEY_RIGHT_CONTROL 345
#define KEY_RIGHT_ALT 346
#define KEY_RIGHT_SUPER 347

#define KEY_RIGHT 262
#define KEY_LEFT 263
#define KEY_DOWN 264
#define KEY_UP 265

#define KEY_ENTER 257
#define KEY_ESCAPE 256

static void on_ui_window_key_release(const int key) {
    ImGuiIO &io = ImGui::GetIO();
    io.KeysDown[key] = false;

    io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];
}

static void on_ui_window_key_press(const int key) {
    ImGuiIO &io = ImGui::GetIO();

    io.KeysDown[key] = true;

    io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];
}

static void on_ui_window_char_type(std::uint32_t c) {
    ImGuiIO &io = ImGui::GetIO();

    if (c > 0 && c < 0x10000) {
        io.AddInputCharacter(static_cast<unsigned short>(c));
    }
}

int ui_debugger_thread() {
    debugger_window = eka2l1::drivers::new_emu_window(eka2l1::drivers::window_type::glfw);

    debugger_window->raw_mouse_event = on_ui_window_mouse_evt;
    debugger_window->mouse_wheeling = on_ui_window_mouse_scrolling;
    debugger_window->button_pressed = on_ui_window_key_press;
    debugger_window->button_released = on_ui_window_key_release;
    debugger_window->char_hook = on_ui_window_char_type;

    std::string window_title = std::string("Debugging Window (") + GIT_BRANCH + " " + 
        GIT_COMMIT_HASH + ")";

    debugger_window->init(window_title, eka2l1::vec2(500, 500));
    debugger_window->make_current();

    eka2l1::drivers::init_graphics_library(eka2l1::drivers::graphic_api::opengl);

    /* Consider main thread not touching this, no need for mutex */
    ui_debugger_context = ImGui::CreateContext();
    auto debugger_renderer = eka2l1::new_debugger_renderer(eka2l1::debugger_renderer_type::opengl);

    /* This will be called by the debug thread */
    debugger->on_pause_toogle = [&](bool spause) {
        if (spause != should_pause) {
            if (spause == false && should_pause == true) {
                should_pause = spause;
                debugger->notify_clients();
            }
        }

        should_pause = spause;
    };

    gdriver = drivers::create_graphics_driver(drivers::graphic_api::opengl,
        eka2l1::vec2(500, 500));

    debugger_renderer->init(gdriver, debugger);

    symsys->set_graphics_driver(gdriver);
    cond.notify_one();

    ImGuiIO &io = ImGui::GetIO();

    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    io.KeyMap[ImGuiKey_Tab] = KEY_TAB;
    io.KeyMap[ImGuiKey_Backspace] = KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_LeftArrow] = KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = KEY_UP;
    io.KeyMap[ImGuiKey_Escape] = KEY_ESCAPE;
    io.KeyMap[ImGuiKey_DownArrow] = KEY_DOWN;
    io.KeyMap[ImGuiKey_Enter] = KEY_ENTER;

    while (!debugger_quit) {
        vec2 nws = debugger_window->window_size();
        vec2 nwsb = debugger_window->window_fb_size();

        debugger_window->poll_events();

        if (debugger_window->should_quit()) {
            should_quit = true;
            debugger_quit = true;

            // Notify that debugger is dead
            debugger->notify_clients();

            break;
        }

        // Stop the emulation but keep the debugger
        if (debugger->should_emulate_stop()) {
            should_quit = true;
        }

        for (std::uint8_t i = 0; i < 5; i++) {
            io.MouseDown[i] = ui_window_mouse_down[i] || debugger_window->get_mouse_button_hold(i);

            set_mouse_down(i, false);
        }

        vec2d mouse_pos = debugger_window->get_mouse_pos();

        io.MousePos = ImVec2(static_cast<float>(mouse_pos[0]), static_cast<float>(mouse_pos[1]));

        debugger_renderer->draw(nws.x, nws.y, nwsb.x, nwsb.y);
        debugger_window->swap_buffer();

        io.MouseWheel = 0;
    }

    ImGui::DestroyContext();

    debugger_renderer->deinit();
    debugger_renderer.reset();

    // Kill the driver before killing the window.
    // We need to destroy all graphics object, if we destroy the window, graphic context is not available
    gdriver.reset();

    return 0;
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
    std::cout << "-------------- EKA2L1: Experimental Symbian Emulator -----------------" << std::endl;

    /* Setup the UI logger. */
    logger = std::make_shared<eka2l1::imgui_logger>();
    log::setup_log(logger);

    read_config();
    parse_args(argc, argv);

    if (should_quit) {
        do_quit();
        return 0;
    }

    eka2l1::drivers::init_window_library(eka2l1::drivers::window_type::glfw);
    debugger = std::make_shared<eka2l1::imgui_debugger>(symsys.get(), logger);

    init();
    do_args();

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
    
    debugger_window->done_current();
    debugger_window->shutdown();

    eka2l1::drivers::destroy_window_library(eka2l1::drivers::window_type::glfw);

    return 0;
}