/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <common/configure.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/vecx.h>
#include <common/thread.h>
#include <common/random.h>
#include <console/state.h>
#include <console/thread.h>
#include <console/seh_handler.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>

#include <epoc/services/window/window.h>
#include <e32keys.h>

void set_mouse_down(void *userdata, const int button, const bool op) {
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);

    const std::lock_guard<std::mutex> guard(emu->input_mutex);
    emu->mouse_down[button] = op;
}

static void on_ui_window_mouse_evt(void *userdata, eka2l1::point mouse_pos, int button, int action) {
    ImGuiIO &io = ImGui::GetIO();
    io.MousePos = ImVec2(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y));

    if (action == 0) {
        set_mouse_down(userdata, button, true);
    }
}

static void on_ui_window_mouse_scrolling(void *userdata, eka2l1::vec2 v) {
    ImGuiIO &io = ImGui::GetIO();
    io.MouseWheel += static_cast<float>(v.y);
}

static eka2l1::drivers::input_event make_key_event_driver(const int key, const eka2l1::drivers::key_state key_state) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::key;
    evt.key_.state_ = key_state;
    evt.key_.code_ = EStdKeyApplication0;       // TODO: Flexible .-.

    return evt;
}

static void on_ui_window_key_release(void *userdata, const int key) {
    ImGuiIO &io = ImGui::GetIO();
    io.KeysDown[key] = false;

    io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];
    
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    auto key_evt = make_key_event_driver(key, eka2l1::drivers::key_state::released);
    
    if (emu->symsys)
        emu->winserv->queue_input_from_driver(key_evt);
}

static void on_ui_window_key_press(void *userdata, const int key) {
    ImGuiIO &io = ImGui::GetIO();

    io.KeysDown[key] = true;

    io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];

    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    auto key_evt = make_key_event_driver(key, eka2l1::drivers::key_state::pressed);

    if (emu->symsys)
        emu->winserv->queue_input_from_driver(key_evt);
}

static void on_ui_window_char_type(void *userdata, std::uint32_t c) {
    ImGuiIO &io = ImGui::GetIO();

    if (c > 0 && c < 0x10000) {
        io.AddInputCharacter(static_cast<unsigned short>(c));
    }
}

namespace eka2l1::desktop {
    static constexpr const char *graphics_driver_thread_name = "Graphics thread";
    static constexpr const char *ui_thread_name = "UI thread";
    static constexpr const char *os_thread_name = "Symbian OS thread";
    static constexpr const char *hli_thread_name = "High level interface thread";

    static int graphics_driver_thread_initialization(emulator &state) {
        if (!drivers::init_window_library(drivers::window_type::glfw)) {
            return -1;
        }

        state.window = drivers::new_emu_window(eka2l1::drivers::window_type::glfw);

        state.window->raw_mouse_event = on_ui_window_mouse_evt;
        state.window->mouse_wheeling = on_ui_window_mouse_scrolling;
        state.window->button_pressed = on_ui_window_key_press;
        state.window->button_released = on_ui_window_key_release;
        state.window->char_hook = on_ui_window_char_type;

        std::string window_title = "EKA2L1 (" GIT_BRANCH " " GIT_COMMIT_HASH ")";

        static constexpr const char *random_references[] = {
            "Get ready, ready to set you off!",
            "A demon lady with a bread in her mouth, shining in the sun",
            "The story of pirates never ends",
            "Uchiha with his brother go to clothes shop in prepare of his real estate job",
            "Living inside the sewer as a clown",
            "Me and the boys discovering this emulator and a bunch of Russians opening another dimension",
            "Having a cyborg as my wife doing dishes and writing the emulator brb"
            // You can add more, but probably when the emulator becomes more functional
        };
        
        constexpr const int random_references_count = sizeof(random_references) / sizeof(const char*);

        window_title += std::string(" - ") + random_references[eka2l1::random_range(0, random_references_count - 1)];

        state.window->init(window_title, eka2l1::vec2(1080, 720));
        state.window->set_userdata(&state);
        state.window->make_current();

        // We got window and context ready (OpenGL, let makes stuff now)
        // TODO: Configurable
        state.graphics_driver = drivers::create_graphics_driver(drivers::graphic_api::opengl);
        state.symsys->set_graphics_driver(state.graphics_driver.get());

        drivers::emu_window *window = state.window.get();

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

        // Signal that the initialization is done
        state.graphics_sema.notify();
        return 0;
    }

    static int graphics_driver_thread_deinitialization(emulator &state) {
        state.graphics_sema.wait();

        state.graphics_driver.reset();
        state.window->shutdown();

        if (!drivers::destroy_window_library(eka2l1::drivers::window_type::glfw)) {
            return -1;
        }

        return 0;
    }

    void graphics_driver_thread(emulator &state) {
        int result = graphics_driver_thread_initialization(state);

        if (result != 0) {
            LOG_ERROR("Graphics driver initialization failed with code {}", result);
            return;
        }

        // Keep running. User which want to change the graphics backend will have to restart EKA2L1.
        state.graphics_driver->run();

        result = graphics_driver_thread_deinitialization(state);

        if (result != 0) {
            LOG_ERROR("Graphics driver deinitialization failed with code {}", result);
            return;
        }
    }

    static int ui_thread_initialization(emulator &state) {
        ImGui::CreateContext();

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

        state.deb_renderer = std::make_unique<debugger_renderer>();

        state.debugger->on_pause_toogle = [&](bool spause) {
            if (spause != state.should_emu_pause) {
                if (spause == false && state.should_emu_pause == true) {
                    state.should_emu_pause = spause;
                    state.debugger->notify_clients();
                }
            }

            state.should_emu_pause = spause;
        };

        // Now wait for the graphics thread before starting anything
        state.graphics_sema.wait();

        return 0;
    }

    static int ui_thread_deinitialization(emulator &state) {
        ImGui::DestroyContext();

        // Build a last command list to destroy resources
        std::unique_ptr<drivers::graphics_command_list> cmd_list = state.graphics_driver->new_command_list();
        std::unique_ptr<drivers::graphics_command_list_builder> cmd_builder = state.graphics_driver->new_command_builder(cmd_list.get());

        state.deb_renderer->deinit(cmd_builder.get());

        // Submit destroy to driver. UI thread resources
        state.graphics_driver->submit_command_list(*cmd_list);

        // Make the graphics driver abort
        state.graphics_driver->abort();
        
        state.launch_requests.abort();

        return 0;
    }

    void ui_thread(emulator &state) {
        int result = ui_thread_initialization(state);

        if (result != 0) {
            LOG_ERROR("UI thread initialization failed with code {}", result);
            return;
        }

        ImGuiIO &io = ImGui::GetIO();

        std::unique_ptr<drivers::graphics_command_list> cmd_list = state.graphics_driver->new_command_list();
        std::unique_ptr<drivers::graphics_command_list_builder> cmd_builder = state.graphics_driver->new_command_builder(cmd_list.get());

        state.deb_renderer->init(state.graphics_driver.get(), cmd_builder.get(), state.debugger.get());

        while (!state.should_ui_quit) {
            const vec2 nws = state.window->window_size();
            const vec2 nwsb = state.window->window_fb_size();

            if (state.window->should_quit()) {
                state.should_emu_quit = true;
                state.should_ui_quit = true;

                // Notify that debugger is dead
                state.debugger->notify_clients();

                break;
            }

            // Stop the emulation but keep the debugger
            if (state.debugger->should_emulate_stop()) {
                state.should_emu_quit = true;
            }

            for (std::uint8_t i = 0; i < 5; i++) {
                io.MouseDown[i] = state.mouse_down[i] || state.window->get_mouse_button_hold(i);
                set_mouse_down(&state, i, false);
            }

            const vec2d mouse_pos = state.window->get_mouse_pos();
            io.MousePos = ImVec2(static_cast<float>(mouse_pos[0]), static_cast<float>(mouse_pos[1]));

            // Render the graphics
            state.deb_renderer->draw(state.graphics_driver.get(), cmd_builder.get(), nws.x, nws.y, nwsb.x, nwsb.y);

            int wait_status = -100;

            // Submit, present, and wait for the presenting
            cmd_builder->present(&wait_status);

            state.graphics_driver->submit_command_list(*cmd_list);
            state.graphics_driver->wait_for(&wait_status);

            io.MouseWheel = 0;
            
            // Recreate the list and builder
            cmd_list = state.graphics_driver->new_command_list();
            cmd_builder = state.graphics_driver->new_command_builder(cmd_list.get());
        }

        result = ui_thread_deinitialization(state);

        if (result != 0) {
            LOG_ERROR("UI thread deinitialization failed with code {}", result);
            return;
        }
    }

    void high_level_interface_thread(emulator &state) {
        std::unique_ptr<std::thread> os_thread_obj;

        while (true) {
            auto launch = state.launch_requests.pop();

            if (!launch) {
                break;
            }

            state.symsys->load(launch.value(), u"");

            if (state.first_time) {
                state.first_time = false;
                
                // Launch the OS thread now
                os_thread_obj = std::make_unique<std::thread>(os_thread, std::ref(state));
                
                // Breath of the dead (jk please dont overreact)
                eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(os_thread_obj->native_handle()),
                    os_thread_name);
            }
        }

        if (os_thread_obj) {
            os_thread_obj->join();
        }

        state.graphics_sema.notify();
    }

    void os_thread(emulator &state) {
        // Register SEH handler for this thread
#if EKA2L1_PLATFORM(WIN32) && defined(_MSC_VER) && ENABLE_SEH_HANDLER
        _set_se_translator(seh_handler_translator_func);
#endif

        // TODO: Multi core. Currently it's single core.
        while (!state.should_emu_quit) {
            try {
                state.symsys->loop();
            } catch (std::exception &exc) {
                std::cout << "Main loop exited with exception: " << exc.what() << std::endl;
                state.debugger->queue_error(exc.what());
                state.should_emu_quit = true;

                break;
            }

            if (state.should_emu_pause && !state.should_emu_quit) {
                state.debugger->wait_for_debugger();
            }
        }

        state.symsys.reset();
    }

    int emulator_entry(emulator &state) {
        state.stage_two();

        // First, initialize the graphics driver. This is needed for all graphics operations on the emulator.
        std::thread graphics_thread_obj(graphics_driver_thread, std::ref(state));
        std::thread ui_thread_obj(ui_thread, std::ref(state));

        std::thread hli_thread_obj(high_level_interface_thread, std::ref(state));

        // Halloween decoration breath of the graphics
        eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(graphics_thread_obj.native_handle()),
            graphics_driver_thread_name);

        // Breath of the UI
        eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(ui_thread_obj.native_handle()),
            ui_thread_name);

        // Breath of the ???
        eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(hli_thread_obj.native_handle()),
            hli_thread_name);

        // Wait for interface thread to be killed.
        hli_thread_obj.join();

        // Wait for the UI to be killed next. Resources of the UI need to be destroyed before ending graphics driver life.
        ui_thread_obj.join();

        // Finally, kill of the graphics driver.
        graphics_thread_obj.join();

        return 0;
    }
}