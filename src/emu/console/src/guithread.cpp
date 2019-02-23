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

#include <console/guithread.h>
#include <console/global.h>
#include <common/cvt.h>
#include <common/configure.h>
#include <common/log.h>
#include <common/vecx.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>

using namespace eka2l1;

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
    auto debugger_window = eka2l1::drivers::new_emu_window(eka2l1::drivers::window_type::glfw);

    debugger_window->raw_mouse_event = on_ui_window_mouse_evt;
    debugger_window->mouse_wheeling = on_ui_window_mouse_scrolling;
    debugger_window->button_pressed = on_ui_window_key_press;
    debugger_window->button_released = on_ui_window_key_release;
    debugger_window->char_hook = on_ui_window_char_type;

    std::string window_title = "Debugging Window (" GIT_BRANCH " " GIT_COMMIT_HASH  ")";

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
            io.MouseDown[i] = ui_window_mouse_down[i] || 
                debugger_window->get_mouse_button_hold(i);

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

    debugger_window->done_current();
    debugger_window->shutdown();

    return 0;
}