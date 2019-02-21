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

#include <common/configure.h>
#include <epoc/epoc.h>
#include <epoc/loader/rom.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>

#include <imgui.h>
#include <yaml-cpp/yaml.h>

#include <gdbstub/gdbstub.h>
#include <manager/manager.h>
#include <manager/device_manager.h>

#include <common/ini.h>
#include <common/arghandler.h>

using namespace eka2l1;

#pragma region GLOBAL_DATA

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
std::shared_ptr<eka2l1::drivers::emu_window> debugger_window;

std::mutex lock;
std::condition_variable cond;

std::uint8_t device_to_use = 0;         ///< Device that will be used

#pragma endregion

#pragma region ARGS_HANDLER_CALLBACK
static bool app_install_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    const char *path = parser->next_token();

    if (!path) {
        *err = "Request to install a SIS, but path not given";
        return false;
    }
    
    // Since it's inconvinient for user to specify the drive (they are all the same on computer),
    // and it's better to install in C since there is many apps required
    // to be in it and hardcoded the drive, just hardcode drive C here.
    bool result = symsys->install_package(common::utf8_to_ucs2(path), drive_c);

    if (!result) {
        *err = "Installation of SIS failed";
        return false;
    }

    return true;
}

static bool app_specifier_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    const char *tok = parser->next_token();

    if (!tok) {
        *err = "No application specified";
        return false;
    }
    
    auto infos = symsys->get_manager_system()->get_package_manager()->get_apps_info();
    std::uint8_t app_idx = 0;

    try {
        app_idx = static_cast<decltype(app_idx)>(std::atoi(tok));
    } catch (...) {
        *err = "Expect an app index after --app, got ";
        *err += tok;

        return false;
    }

    bool result = symsys->load(infos[app_idx].id);

    if (!result) {
        *err = "Application load failed";
        return false;
    }

    return true;
}

static bool help_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    std::cout << parser->get_help_string();
    return false;
}

static bool rpkg_unpack_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    // Base Z drive is mount_z
    // The RPKG path is the next token
    const char *path = parser->next_token();

    if (!path) {
        *err = "RPKG installation failed. No path provided";
        return false;
    }

    bool install_result = symsys->install_rpkg(mount_z, path);
    if (!install_result) {
        *err = "RPKG installation failed. Something is wrong, see log";
        return false;
    }

    return false;
}

static bool list_app_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    auto infos = symsys->get_manager_system()->get_package_manager()->get_apps_info();

    for (auto &info : infos) {
        std::cout << "[0x" << common::to_string(info.id, std::hex) << "]: "
                    << common::ucs2_to_utf8(info.name) << " (drive: " << static_cast<char>('A' + info.drive)
                    << " , executable name: " << common::ucs2_to_utf8(info.executable_name) << ")" << std::endl;
    }

    return false;
}

static bool list_devices_option_handler(eka2l1::common::arg_parser *parser, std::string *err) {
    auto &devices_list = symsys->get_manager_system()->get_device_manager()->get_devices();

    for (std::size_t i = 0; i < devices_list.size(); i++) {
        std::cout << i << " : " << devices_list[i].model << " (" << devices_list[i].firmware_code << ", "
            << devices_list[i].model << ", epocver:";

        switch (devices_list[i].ver) {
        case epocver::epocu6: {
            std::cout << " under 6.0";
            break;
        }

        case epocver::epoc6: {
            std::cout << " 6.0";
            break;
        }
        
        case epocver::epoc93: {
            std::cout << " 9.3";
            break;
        }
        
        case epocver::epoc94: {
            std::cout << " 9.4";
            break;
        }

        case epocver::epoc10: {
            std::cout << " 10.0";
            break;
        }

        default: {
            std::cout << " Unknown";
            break;
        }
        }

        std::cout << ")" << std::endl;
    }

    return false;
}

#pragma endregion

#pragma region CONFIGS
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

#pragma endregion

#pragma region CORE_FINALIZE_INITIALIZE

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

#pragma endregion

#pragma region DEBUGGER

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

    std::string window_title = std::string("Debugging Window (") + GIT_BRANCH + " " + GIT_COMMIT_HASH + ")";

    debugger_window->init(window_title, eka2l1::vec2(1080, 720));
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

    auto gdriver = drivers::create_graphics_driver(drivers::graphic_api::opengl,
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

    // There are still a reference of graphics driver on graphics client
    // Assure that the graphics driver are decrement is reference count on UI thread
    // So FB is destroyed on the right thread.
    symsys->set_graphics_driver(nullptr);

    return 0;
}

#pragma endregion

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

    debugger_window->done_current();
    debugger_window->shutdown();

    eka2l1::drivers::destroy_window_library(eka2l1::drivers::window_type::glfw);

    return 0;
}