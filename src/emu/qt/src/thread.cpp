/*
 * Copyright (c) 2019 EKA2L1 Team.
 * Copyright 2015 Dolphin Emulator Project.
 * 
 * This file is part of EKA2L1 project.
 * 
 * A portion of the code is borrowed from MainWindow.cpp in DolphinQt
 * source folder.
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

#include <common/arghandler.h>
#include <common/configure.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/random.h>
#include <common/thread.h>
#include <common/time.h>
#include <common/vecx.h>
#include <qt/cmdhandler.h>
#include <qt/displaywidget.h>
#include <qt/seh_handler.h>
#include <qt/state.h>
#include <qt/thread.h>
#include <qt/utils.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>
#include <drivers/input/emu_controller.h>

#include <services/window/window.h>
#include <qt/input_dialog_driver.h>

#include <kernel/kernel.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#include <QApplication>
#include <QWindow>

#include <qt/mainwindow.h>
#include <iostream>

static eka2l1::drivers::input_event make_mouse_event_driver(const float x, const float y, const float z, const int button, const int action,
    const int mouse_id) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::touch;
    evt.mouse_.pos_x_ = static_cast<int>(x);
    evt.mouse_.pos_y_ = static_cast<int>(y);
    evt.mouse_.pos_z_ = static_cast<int>(z);
    evt.mouse_.mouse_id = static_cast<std::uint32_t>(mouse_id);
    evt.mouse_.button_ = static_cast<eka2l1::drivers::mouse_button>(button);
    evt.mouse_.action_ = static_cast<eka2l1::drivers::mouse_action>(action);

    return evt;
}

/**
 * \brief Callback when a host mouse event is triggered
 * \param mouse_pos    position of mouse pointer          
 * \param button       0: left, 1: right, 2: other      
 * \param action       0: press, 1: repeat(move), 2: release      
 */
static void on_ui_window_mouse_evt(void *userdata, eka2l1::vec3 mouse_pos, int button, int action, int mouse_id) {
    float mouse_pos_x = static_cast<float>(mouse_pos.x), mouse_pos_y = static_cast<float>(mouse_pos.y),
        mouse_pos_z = static_cast<float>(mouse_pos.z);

    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);

    const std::lock_guard<std::mutex> guard(emu->lockdown);

    const float scale = emu->symsys->get_config()->ui_scale;
    auto mouse_evt = make_mouse_event_driver(mouse_pos_x / scale, mouse_pos_y / scale, mouse_pos_z / scale,
        button, action, mouse_id);

    if ((emu->symsys) && emu->winserv) {
        emu->winserv->queue_input_from_driver(mouse_evt);
    }
}

static eka2l1::drivers::input_event make_controller_event_driver(int jid, int button, bool is_press) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::button;
    evt.button_.button_ = button;
    evt.button_.controller_ = jid;
    evt.button_.state_ = is_press ? eka2l1::drivers::button_state::pressed : eka2l1::drivers::button_state::released;

    return evt;
}

static eka2l1::drivers::input_event make_key_event_driver(const int key, const eka2l1::drivers::key_state key_state) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::key;
    evt.key_.state_ = key_state;
    evt.key_.code_ = key;

    return evt;
}

static void on_ui_window_key_release(void *userdata, const int key) {
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    auto key_evt = make_key_event_driver(key, eka2l1::drivers::key_state::released);

    const std::lock_guard<std::mutex> guard(emu->lockdown);
    if (emu->winserv)
        emu->winserv->queue_input_from_driver(key_evt);
}

static void on_ui_window_key_press(void *userdata, const int key) {
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    auto key_evt = make_key_event_driver(key, eka2l1::drivers::key_state::pressed);

    const std::lock_guard<std::mutex> guard(emu->lockdown);
    if (emu->winserv)
        emu->winserv->queue_input_from_driver(key_evt);
}

namespace eka2l1::desktop {
    static constexpr const char *graphics_driver_thread_name = "Graphics thread";
    static constexpr const char *os_thread_name = "Symbian OS thread";

    static int graphics_driver_thread_initialization(emulator &state) {
        // Halloween decoration breath of the graphics
        eka2l1::common::set_thread_name(graphics_driver_thread_name);
        eka2l1::common::set_thread_priority(eka2l1::common::thread_priority_high);

        state.window->raw_mouse_event = on_ui_window_mouse_evt;
        state.window->button_pressed = on_ui_window_key_press;
        state.window->button_released = on_ui_window_key_release;

        state.window->init("Emulator display", eka2l1::vec2(800, 600), drivers::emu_window_flag_maximum_size);
        state.window->set_userdata(&state);

        // We got window and context ready (OpenGL, let makes stuff now)
        // TODO: Configurable
        state.graphics_driver = drivers::create_graphics_driver(drivers::graphic_api::opengl, state.window->get_window_system_info());
        state.symsys->set_graphics_driver(state.graphics_driver.get());

        drivers::emu_window *window = state.window;

        switch (state.graphics_driver->get_current_api()) {
        case drivers::graphic_api::opengl: {
            state.graphics_driver->set_display_hook([window]() {
                window->swap_buffer();
                window->poll_events();
            });

            break;
        }

        default: {
            state.graphics_driver->set_display_hook([window]() {
                window->poll_events();
            });

            break;
        }
        }

        state.joystick_controller = drivers::new_emu_controller(eka2l1::drivers::controller_type::sdl2);
        state.joystick_controller->on_button_event = [&](int jid, int button, bool pressed) {
            const std::lock_guard<std::mutex> guard(state.lockdown);
            auto evt = make_controller_event_driver(jid, button, pressed);

            // If the handler accepts it
            if (state.ui_main->controller_event_handler(evt) && state.winserv) {
                state.winserv->queue_input_from_driver(evt);
            }
        };

        // Signal that the initialization is done
        state.graphics_event.set();
        return 0;
    }

    static int graphics_driver_thread_deinitialization(emulator &state) {
        if (state.stage_two_inited)
            state.graphics_event.wait();

        state.joystick_controller->stop_polling();
        state.graphics_driver.reset();

        return 0;
    }

    void graphics_driver_thread(emulator &state) {
        int result = graphics_driver_thread_initialization(state);

        if (result != 0) {
            LOG_ERROR(FRONTEND_CMDLINE, "Graphics driver initialization failed with code {}", result);
            return;
        }

        state.joystick_controller->start_polling();

        // Keep running. User which want to change the graphics backend will have to restart EKA2L1.
        state.graphics_event.reset();
        state.graphics_driver->run();

        result = graphics_driver_thread_deinitialization(state);

        if (result != 0) {
            LOG_ERROR(FRONTEND_CMDLINE, "Graphics driver deinitialization failed with code {}", result);
            return;
        }
    }

    void os_thread(emulator &state) {
#if EKA2L1_PLATFORM(WIN32)
        HRESULT hr = S_OK;
        hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        if (hr != S_OK) {
            LOG_CRITICAL(FRONTEND_CMDLINE, "Failed to initialize COM");
            return;
        }
#endif

        eka2l1::common::set_thread_name(os_thread_name);
        eka2l1::common::set_thread_priority(eka2l1::common::thread_priority_high);

        bool first_time = true;

        while (true) {
            if (state.should_emu_quit) {
                break;
            }

            const bool success = state.stage_two();
            state.init_event.set();

            if (first_time) {
                state.graphics_event.wait();
                first_time = false;
            }

            if (success) {
                break;
            }

            // Try wait for initialization from other parties to make this success.
            state.init_event.reset();
            state.init_event.wait();
        }

        // Register SEH handler for this thread
#if EKA2L1_PLATFORM(WIN32) && defined(_MSC_VER) && ENABLE_SEH_HANDLER
        _set_se_translator(seh_handler_translator_func);
#endif

        while (!state.should_emu_quit) {
#if ENABLE_SEH_HANDLER
            try {
#endif
                state.symsys->loop();
#if ENABLE_SEH_HANDLER
            } catch (std::exception &exc) {
                std::cout << "Main loop exited with exception: " << exc.what() << std::endl;
                // TODO： Come back and make this display in UI
                // state.debugger->queue_error(exc.what());
                state.should_emu_quit = true;
                break;
            }
#endif

            if (state.should_emu_pause && !state.should_emu_quit) {
                state.pause_event.wait();
                state.pause_event.reset();
            }
        }

        state.kill_event.wait();
        state.symsys.reset();
        state.graphics_event.set();

#if EKA2L1_PLATFORM(WIN32)
        CoUninitialize();
#endif
    }

    void kill_emulator(emulator &state) {
        state.should_emu_quit = true;
        state.should_emu_pause = false;

        state.pause_event.set();

        kernel_system *kern = state.symsys->get_kernel_system();

        if (kern) {
            kern->stop_cores_idling();
        }

        state.graphics_driver->abort();
        state.init_event.set();
        state.kill_event.set();
    }

    int emulator_entry(QApplication &application, emulator &state, const int argc, const char **argv) {
        state.stage_one();

        // Instantiate UI and High-level interface threads
        std::thread os_thread_obj(os_thread, std::ref(state));
        state.init_event.wait();

        eka2l1::common::arg_parser parser(argc, argv);

        parser.add("--help, -h", "Display helps menu", help_option_handler);
        parser.add("--listapp", "List all installed applications", list_app_option_handler);
        parser.add("--listdevices", "List all installed devices", list_devices_option_handler);
        parser.add("--app, -a, --run", "Run an app with given name or UID, or the absolute virtual path to executable.\n"
                                        "\t\t\t  See list of apps with --listapp.\n"
                                        "\t\t\t  Extra command line arguments can be passed to the application.\n"
                                        "\n"
                                        "\t\t\t  Some example:\n"
                                        "\t\t\t    eka2l1 --run C:\\sys\\bin\\BitmapTest.exe \"--hi --arg 5\"\n"
                                        "\t\t\t    eka2l1 --run Bounce\n"
                                        "\t\t\t    eka2l1 --run 0x200412ED\n",
            app_specifier_option_handler);
        parser.add("--device, -dvc", "Set a device to be ran, through the given firmware code. This device will also be saved in the configuration as the current device.\n"
                               "\t\t\t Example: --device RH-29",
            device_set_option_handler);
        parser.add("--install, -i", "Install a SIS.", app_install_option_handler);
        parser.add("--remove, -r", "Remove an package.", package_remove_option_handler);
        parser.add("--fullscreen, -f", "Display the emulator in fullscreen.", fullscreen_option_handler);
        parser.add("--mount, -m", "Load a folder/zip as a Game Card ROM.", mount_card_option_handler);
        parser.add("--keybindprofile, -kbp", "Set a keybind profile to associate with the emulator launch. Don't include any file extension here.\n"
                                              "\t Example: eka2l1 --kbp controller_for_octopus",
            keybind_profile_option_handler);

#if ENABLE_PYTHON_SCRIPTING
        parser.add("--gendocs", "Generate Python documentation", python_docgen_option_handler);
#endif

        if (argc > 1) {
            std::string err;
            state.should_emu_quit = !parser.parse(&state, &err);

            if (state.should_emu_quit) {
                // Notify the OS thread that is still sleeping, waiting for
                // graphics sema to be freed.
                state.graphics_event.set();

                std::cout << err << std::endl;
                os_thread_obj.join();

                return -1;
            }
        }

        state.ui_main = new main_window(application, nullptr, state);
        state.ui_main->show();
        state.ui_main->setWindowTitle(get_emulator_window_title());

        eka2l1::drivers::ui::main_window_instance = state.ui_main;
        state.window = state.ui_main->render_window();

        std::thread graphics_thread_obj(graphics_driver_thread, std::ref(state));

        if (state.init_app_launched) {
            state.ui_main->setup_and_switch_to_game_mode();
        }

        const int exec_code = application.exec();
        kill_emulator(state);

        // Wait for OS thread to die
        os_thread_obj.join();
        graphics_thread_obj.join();

        delete state.ui_main;
        return exec_code;
    }
}
