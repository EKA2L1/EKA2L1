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
#include <common/random.h>
#include <common/thread.h>
#include <common/time.h>
#include <common/vecx.h>
#include <common/version.h>
#include <console/seh_handler.h>
#include <console/state.h>
#include <console/thread.h>

#include <debugger/imgui_debugger.h>
#include <debugger/logger.h>
#include <debugger/renderer/renderer.h>

#include <drivers/graphics/emu_window.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>
#include <drivers/input/emu_controller.h>

#include <e32keys.h>
#include <services/window/window.h>

void set_mouse_down(void *userdata, const int button, const bool op) {
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);

    const std::lock_guard<std::mutex> guard(emu->input_mutex);
    emu->mouse_down[button] = op;
}

static eka2l1::drivers::input_event make_mouse_event_driver(const float x, const float y, const int button, const int action) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::touch;
    evt.mouse_.pos_x_ = static_cast<int>(x);
    evt.mouse_.pos_y_ = static_cast<int>(y);
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
static void on_ui_window_mouse_evt(void *userdata, eka2l1::point mouse_pos, int button, int action) {
    float mouse_pos_x = static_cast<float>(mouse_pos.x), mouse_pos_y = static_cast<float>(mouse_pos.y);

    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    auto mouse_evt = make_mouse_event_driver(mouse_pos_x, mouse_pos_y, button, action);

    if ((emu->symsys) && emu->winserv) {
        const float scale = emu->symsys->get_config()->ui_scale;
        mouse_pos_x /= scale;
        mouse_pos_y /= scale;

        emu->winserv->queue_input_from_driver(mouse_evt);
    }

    if (emu->debugger->request_key && !emu->debugger->key_set) {
        emu->debugger->key_evt = mouse_evt;
        emu->debugger->key_set = true;
    }

    ImGuiIO &io = ImGui::GetIO();
    io.MousePos = ImVec2(mouse_pos_x, mouse_pos_y);

    if (action == 0) {
        set_mouse_down(userdata, button, true);
    }
}

static eka2l1::drivers::input_event on_controller_button_event(eka2l1::window_server *winserv, int jid, int button, bool is_press) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::button;
    evt.button_.button_ = button;
    evt.button_.controller_ = jid;
    evt.button_.state_ = is_press ? eka2l1::drivers::button_state::pressed : eka2l1::drivers::button_state::released;
    if (winserv) winserv->queue_input_from_driver(evt);
    return evt;
}

static void on_ui_window_mouse_scrolling(void *userdata, eka2l1::vec2d v) {
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    emu->mouse_scroll = v;
}

static void on_ui_window_touch_move(void *userdata, eka2l1::vec2 v) {
    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);

    // Make repeat event for button 0 (left mouse click)
    auto mouse_evt = make_mouse_event_driver(static_cast<float>(v.x), static_cast<float>(v.y), 0, 1);

    if (emu->winserv)
        emu->winserv->queue_input_from_driver(mouse_evt);
}

static eka2l1::drivers::input_event make_key_event_driver(const int key, const eka2l1::drivers::key_state key_state) {
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::key;
    evt.key_.state_ = key_state;
    evt.key_.code_ = key;

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

    if (emu->winserv)
        emu->winserv->queue_input_from_driver(key_evt);
}

static eka2l1::drivers::input_event on_ui_window_key_press(void *userdata, const int key) {
    ImGuiIO &io = ImGui::GetIO();

    io.KeysDown[key] = true;

    io.KeyCtrl = io.KeysDown[KEY_LEFT_CONTROL] || io.KeysDown[KEY_RIGHT_CONTROL];
    io.KeyShift = io.KeysDown[KEY_LEFT_SHIFT] || io.KeysDown[KEY_RIGHT_SHIFT];
    io.KeyAlt = io.KeysDown[KEY_LEFT_ALT] || io.KeysDown[KEY_RIGHT_ALT];
    io.KeySuper = io.KeysDown[KEY_LEFT_SUPER] || io.KeysDown[KEY_RIGHT_SUPER];

    eka2l1::desktop::emulator *emu = reinterpret_cast<eka2l1::desktop::emulator *>(userdata);
    auto key_evt = make_key_event_driver(key, eka2l1::drivers::key_state::pressed);

    if (emu->winserv)
        emu->winserv->queue_input_from_driver(key_evt);

    return key_evt;
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
        // Halloween decoration breath of the graphics
        eka2l1::common::set_thread_name(graphics_driver_thread_name);

        if (!drivers::init_window_library(drivers::window_api::glfw)) {
            return -1;
        }

        state.mouse_cursor_controller = drivers::make_new_cursor_controller(eka2l1::drivers::window_api::glfw);

        state.mouse_cursors[ImGuiMouseCursor_Arrow] = state.mouse_cursor_controller->create(drivers::cursor_type_arrow);
        state.mouse_cursors[ImGuiMouseCursor_TextInput] = state.mouse_cursor_controller->create(drivers::cursor_type_ibeam);
        state.mouse_cursors[ImGuiMouseCursor_ResizeNS] = state.mouse_cursor_controller->create(drivers::cursor_type_vresize);
        state.mouse_cursors[ImGuiMouseCursor_ResizeEW] = state.mouse_cursor_controller->create(drivers::cursor_type_hresize);
        state.mouse_cursors[ImGuiMouseCursor_Hand] = state.mouse_cursor_controller->create(drivers::cursor_type_hand);
        state.mouse_cursors[ImGuiMouseCursor_ResizeAll] = state.mouse_cursor_controller->create(drivers::cursor_type_allresize);
        state.mouse_cursors[ImGuiMouseCursor_ResizeNESW] = state.mouse_cursor_controller->create(drivers::cursor_type_nesw_resize);
        state.mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = state.mouse_cursor_controller->create(drivers::cursor_type_nwse_resize);
        state.mouse_cursors[ImGuiMouseCursor_NotAllowed] = state.mouse_cursor_controller->create(drivers::cursor_type_arrow);

        state.window = drivers::new_emu_window(eka2l1::drivers::window_api::glfw);

        state.window->raw_mouse_event = on_ui_window_mouse_evt;
        state.window->mouse_wheeling = on_ui_window_mouse_scrolling;
        state.window->button_pressed = [&](void *userdata, const int key) {
            auto evt = on_ui_window_key_press(userdata, key);
            if (state.debugger->request_key && !state.debugger->key_set) {
                state.debugger->key_evt = evt;
                state.debugger->key_set = true;
            }
        };
        state.window->button_released = on_ui_window_key_release;
        state.window->char_hook = on_ui_window_char_type;
        state.window->touch_move = on_ui_window_touch_move;

        std::string window_title = "EKA2L1 (" GIT_BRANCH " " GIT_COMMIT_HASH ")";

        static constexpr const char *random_references[] = {
            "Get ready, ready to set you off!",
            "A demon lady with a bread in her mouth, shining in the sun",
            "The story of pirates never ends",
            "Uchiha with his brother go to clothes shop in prepare of his real estate job",
            "Living inside the sewer as a clown",
            "Me and the boys discovering this emulator and a bunch of Russians opening another dimension",
            "Having a cyborg as my wife doing dishes and writing the emulator brb",
            "Causing an entire country chaos because of my imagination",
            "Thank you very much for checking out the emulator",
            "Casually the cause of case files over two decades while staying first-grade"
            "Stop right there criminal scum!",
            "By Azura By Azura By Azura!",
            "VAC is activating... It's Virtual Assistant Cellphone though, so keep using cheats!",
            "Driftin' to save my sister in nowhere!",
            "Will this become an Inferno Arch core soon?",
            "Emulator from nowhere. Now on Netflix.",
            "E3 2005 Prototype version."
            // You can add more, but probably when the emulator becomes more functional
        };

        constexpr const int random_references_count = sizeof(random_references) / sizeof(const char *);

        window_title += std::string(" - ") + random_references[eka2l1::random_range(0, random_references_count - 1)];

        state.window->init(window_title, eka2l1::vec2(1080, 720), drivers::emu_window_flag_maximum_size);
        state.window->set_userdata(&state);
        state.window->make_current();

        // We got window and context ready (OpenGL, let makes stuff now)
        // TODO: Configurable
        state.graphics_driver = drivers::create_graphics_driver(drivers::graphic_api::opengl);
        state.symsys->set_graphics_driver(state.graphics_driver.get());

        drivers::emu_window *window = state.window.get();

        switch (state.graphics_driver->get_current_api()) {
        case drivers::graphic_api::opengl: {
            state.graphics_driver->set_display_hook([window, &state]() {
                window->set_fullscreen(state.deb_renderer->is_fullscreen());

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

        state.joystick_controller = drivers::new_emu_controller(eka2l1::drivers::controller_type::glfw);
        state.joystick_controller->on_button_event = [&](int jid, int button, bool pressed) {
            auto evt = on_controller_button_event(state.winserv, jid, button, pressed);
            if (state.debugger->request_key && !state.debugger->key_set) {
                state.debugger->key_evt = evt;
                state.debugger->key_set = true;
            }
        };

        // Signal that the initialization is done
        state.graphics_sema.notify(2);
        return 0;
    }

    static int graphics_driver_thread_deinitialization(emulator &state) {
        if (state.stage_two_inited)
            state.graphics_sema.wait();

        state.joystick_controller->stop_polling();
        state.graphics_driver.reset();
        state.window->shutdown();

        if (!drivers::destroy_window_library(eka2l1::drivers::window_api::glfw)) {
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

        state.joystick_controller->start_polling();
        // Keep running. User which want to change the graphics backend will have to restart EKA2L1.
        state.graphics_driver->run();

        result = graphics_driver_thread_deinitialization(state);

        if (result != 0) {
            LOG_ERROR("Graphics driver deinitialization failed with code {}", result);
            return;
        }
    }

    static const char *DEFAULT_FONT_PATH = "resources\\mplus-1m-bold.ttf";
    static const float DEFAULT_FONT_SIZE = 16;

    static int ui_thread_initialization(emulator &state) {
        // Breath of the UI
        eka2l1::common::set_thread_name(ui_thread_name);

        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();

        static const ImWchar ranges[] =
        {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
            0x2DE0, 0x2DFF, // Cyrillic Extended-A
            0xA640, 0xA69F, // Cyrillic Extended-B
            0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
            0x31F0, 0x31FF, // Katakana Phonetic Extensions
            0xFF00, 0xFFEF, // Half-width characters
            0x2000, 0x206F, // General Punctuation
            0x0102, 0x0103, // Vietnamese
            0x0110, 0x0111,
            0x0128, 0x0129,
            0x0168, 0x0169,
            0x01A0, 0x01A1,
            0x01AF, 0x01B0,
            0x1EA0, 0x1EF9,
            0,
        };

        if (!io.Fonts->AddFontFromFileTTF(DEFAULT_FONT_PATH, DEFAULT_FONT_SIZE, nullptr, ranges)) {
            io.Fonts->AddFontDefault();
        }

        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendPlatformName = "EKA2L1";

        io.ConfigWindowsMoveFromTitleBarOnly = true;
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
                if (spause) {
                    state.symsys->pause();
                } else {
                    state.symsys->unpause();
                }

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
        
        state.deb_renderer->set_fullscreen(state.init_fullscreen);
        state.debugger->set_logger_visbility(!state.init_fullscreen);

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
            io.MousePos = ImVec2(static_cast<float>(mouse_pos[0] / state.conf.ui_scale), static_cast<float>(mouse_pos[1] / state.conf.ui_scale));

            io.MouseWheelH = static_cast<float>(state.mouse_scroll[0]);
            io.MouseWheel = static_cast<float>(state.mouse_scroll[1]);

            state.mouse_scroll = { 0.0, 0.0 };

            if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) {
                ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
                if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
                    // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
                    state.window->cursor_visiblity(false);
                } else {
                    state.window->set_cursor(state.mouse_cursors[imgui_cursor] ? state.mouse_cursors[imgui_cursor].get() :
                        state.mouse_cursors[ImGuiMouseCursor_Arrow].get());

                    state.window->cursor_visiblity(true);
                }
            }
            
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
        eka2l1::common::set_thread_name(os_thread_name);
        state.graphics_sema.wait();

        // Register SEH handler for this thread
#if EKA2L1_PLATFORM(WIN32) && defined(_MSC_VER) && ENABLE_SEH_HANDLER && !defined(NDEBUG)
        _set_se_translator(seh_handler_translator_func);
#endif

        while (!state.should_emu_quit) {
#if !defined(NDEBUG)
            try {
#endif
                state.symsys->loop();
#if !defined(NDEBUG)
            } catch (std::exception &exc) {
                std::cout << "Main loop exited with exception: " << exc.what() << std::endl;
                state.debugger->queue_error(exc.what());
                state.should_emu_quit = true;
                break;
            }
#endif

            if (state.should_emu_pause && !state.should_emu_quit) {
                state.debugger->wait_for_debugger();
            }
        }

        state.symsys.reset();
        state.graphics_sema.notify();
    }

    int emulator_entry(emulator &state) {
        const bool result = state.stage_two();

        // Instantiate UI and High-level interface threads
        std::thread ui_thread_obj(ui_thread, std::ref(state));
        std::unique_ptr<std::thread> os_thread_obj;
        
        if (result)
            os_thread_obj = std::make_unique<std::thread>(os_thread, std::ref(state));

        // Run graphics driver on main entry.
        graphics_driver_thread(state);

        // Wait for OS thread to die
        if (os_thread_obj) {
            os_thread_obj->join();
        }

        // Wait for the UI to be killed next. Resources of the UI need to be destroyed before ending graphics driver life.
        ui_thread_obj.join();

        return 0;
    }
}
