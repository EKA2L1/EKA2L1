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
#include <console/state.h>
#include <console/thread.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>

#include <epoc/services/window/window.h>
#include <e32keys.h>

/*
using namespace eka2l1;

std::shared_ptr<drivers::input_driver> idriver;


int ui_debugger_thread() {
    auto debugger_window = eka2l1::drivers::new_emu_window(eka2l1::drivers::window_type::glfw);

    debugger_window->raw_mouse_event = on_ui_window_mouse_evt;
    debugger_window->mouse_wheeling = on_ui_window_mouse_scrolling;
    debugger_window->button_pressed = on_ui_window_key_press;
    debugger_window->button_released = on_ui_window_key_release;
    debugger_window->char_hook = on_ui_window_char_type;

    std::string window_title = "Debugging Window (" GIT_BRANCH " " GIT_COMMIT_HASH ")";

    debugger_window->init(window_title, eka2l1::vec2(1080, 720));
    debugger_window->make_current();
    */
/* Consider main thread not touching this, no need for mutex */
//ui_debugger_context = ImGui::CreateContext();
//auto drenderer = std::make_shared<eka2l1::debugger_renderer>();

/* This will be called by the debug thread */
/*debugger->on_pause_toogle = [&](bool spause) {
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

    idriver = std::make_shared<drivers::input_driver>();

    drenderer->init(drivers::graphic_api::opengl, gdriver, idriver, debugger);

    symsys->set_graphics_driver(gdriver);
    symsys->set_input_driver(idriver);

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

        drenderer->draw(nws.x, nws.y, nwsb.x, nwsb.y);
        debugger_window->swap_buffer();

        io.MouseWheel = 0;
    }

    ImGui::DestroyContext();

    drenderer->deinit();
    drenderer.reset();

    // There are still a reference of graphics driver on graphics client
    // Assure that the graphics driver are decrement is reference count on UI thread
    // So FB is destroyed on the right thread.
    symsys->set_graphics_driver(nullptr);
    symsys->set_input_driver(nullptr);

    debugger_window->done_current();
    debugger_window->shutdown();

    return 0;
}
*/

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
    emu->winserv->queue_input_from_driver(key_evt);
}

static void on_ui_window_char_type(void *userdata, std::uint32_t c) {
    ImGuiIO &io = ImGui::GetIO();

    if (c > 0 && c < 0x10000) {
        io.AddInputCharacter(static_cast<unsigned short>(c));
    }
}

namespace eka2l1::desktop {
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

        state.window->init(window_title, eka2l1::vec2(1080, 720));
        state.window->set_userdata(&state);
        state.window->make_current();

        // We got window and context ready (OpenGL, let makes stuff now)
        // TODO: Configurable
        state.graphics_driver = drivers::create_graphics_driver(drivers::graphic_api::opengl);

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
        state.graphics_cond.notify_all();
        return 0;
    }

    static int graphics_driver_thread_deinitialization(emulator &state) {
        if (!drivers::destroy_window_library(eka2l1::drivers::window_type::glfw)) {
            return -1;
        }

        state.window->shutdown();

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
        std::unique_lock<std::mutex> ulock(state.graphics_mutex);
        state.graphics_cond.wait(ulock);

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

    void os_thread(emulator &state) {
        // TODO: Multi core. Currently it's single core.
        while (!state.should_emu_quit) {
            try {
                state.symsys->loop();
            } catch (std::exception &exc) {
                std::cout << "Main loop exited with exception: " << exc.what() << std::endl;
                state.should_emu_quit = true;

                break;
            }

            if (state.should_emu_pause && !state.should_emu_quit) {
                state.debugger->wait_for_debugger();
            }
        }
    }

    static constexpr const char *graphics_driver_thread_name = "Graphics thread";
    static constexpr const char *ui_thread_name = "UI thread";
    static constexpr const char *os_thread_name = "Symbian OS thread";

    int emulator_entry(emulator &state) {
        // First, initialize the graphics driver. This is needed for all graphics operations on the emulator.
        std::thread graphics_thread_obj(graphics_driver_thread, std::ref(state));
        std::thread ui_thread_obj(ui_thread, std::ref(state));

        // Launch the OS thread now
        std::thread os_thread_obj(os_thread, std::ref(state));

        // Halloween decoration breath of the graphics
        eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(graphics_thread_obj.native_handle()),
            graphics_driver_thread_name);

        // Breath of the UI
        eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(ui_thread_obj.native_handle()),
            ui_thread_name);

        // Breath of the dead (jk please dont overreact)
        eka2l1::common::set_thread_name(reinterpret_cast<std::uint64_t>(os_thread_obj.native_handle()),
            os_thread_name);

        // Wait for the OS thread to be killed now
        os_thread_obj.join();

        // Wait for the UI to be killed next. Resources of the UI need to be destroyed before ending graphics driver life.
        ui_thread_obj.join();

        // Finally, kill of the graphics driver.
        graphics_thread_obj.join();

        return 0;
    }
}