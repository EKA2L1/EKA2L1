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

#include <android/thread.h>
#include <common/thread.h>
#include <drivers/graphics/graphics.h>

std::unique_ptr<std::thread> os_thread_obj;
std::unique_ptr<std::thread> ui_thread_obj;
std::unique_ptr<std::thread> gr_thread_obj;

namespace eka2l1::android {
    static constexpr const char *os_thread_name = "Symbian OS thread";
    static constexpr const char *ui_thread_name = "UI thread";
    static constexpr const char *graphics_driver_thread_name = "Graphics thread";

    static int graphics_driver_thread_initialization(emulator &state) {
        // Halloween decoration breath of the graphics
        eka2l1::common::set_thread_name(graphics_driver_thread_name);

        state.window = std::make_unique<drivers::emu_window_android>();
        state.window->init("Hello there", eka2l1::vec2(0, 0),
                           drivers::emu_window_flag_maximum_size);
        state.window->set_userdata(&state);

        state.window->init_gl();

        // We got window and context ready (OpenGL, let makes stuff now)
        // TODO: Configurable
        state.graphics_driver = drivers::create_graphics_driver(drivers::graphic_api::opengl);
        state.symsys->set_graphics_driver(state.graphics_driver.get());

        drivers::emu_window_android *window = state.window.get();
        state.graphics_driver->set_display_hook([window, &state]() {
            if (!state.surface_inited) {
                state.window->init_surface();
                state.surface_inited = true;
            }

            window->swap_buffer();
            window->poll_events();

            if (state.should_graphics_pause) {
                window->destroy_surface();
                state.pause_graphics_sema.wait();

                window->create_surface();
                window->make_current();
                state.pause_sema.notify();
            }
        });
        return 0;
    }

    static int graphics_driver_thread_deinitialization(emulator &state) {
        if (state.stage_two_inited)
            state.graphics_sema.wait();

        state.graphics_driver.reset();

        return 0;
    }

    void graphics_driver_thread(emulator &state) {
        int result = graphics_driver_thread_initialization(state);

        if (result != 0) {
            LOG_ERROR(FRONTEND_CMDLINE, "Graphics driver initialization failed with code {}", result);
            return;
        }

        // Keep running. User which want to change the graphics backend will have to restart EKA2L1.
        state.graphics_driver->run();

        result = graphics_driver_thread_deinitialization(state);

        if (result != 0) {
            LOG_ERROR(FRONTEND_CMDLINE, "Graphics driver deinitialization failed with code {}", result);
            return;
        }
    }

    static int ui_thread_initialization(emulator &state) {
        // Breath of the UI
        eka2l1::common::set_thread_name(ui_thread_name);

        // Now wait for the graphics thread before starting anything
        state.graphics_sema.wait();

        return 0;
    }

    static int ui_thread_deinitialization(emulator &state) {
        // Build a last command list to destroy resources
        std::unique_ptr<drivers::graphics_command_list> cmd_list = state.graphics_driver->new_command_list();
        std::unique_ptr<drivers::graphics_command_list_builder> cmd_builder = state.graphics_driver->new_command_builder(
                cmd_list.get());

        // Submit destroy to driver. UI thread resources
        state.graphics_driver->submit_command_list(*cmd_list);

        // Make the graphics driver abort
        state.graphics_driver->abort();

        return 0;
    }

    void ui_thread(emulator &state) {
        int result = ui_thread_initialization(state);

        if (result != 0) {
            LOG_ERROR(FRONTEND_CMDLINE, "UI thread initialization failed with code {}", result);
            return;
        }

        std::unique_ptr<drivers::graphics_command_list> cmd_list = state.graphics_driver->new_command_list();
        std::unique_ptr<drivers::graphics_command_list_builder> cmd_builder = state.graphics_driver->new_command_builder(
                cmd_list.get());

        while (!state.should_ui_quit) {
            state.launcher->draw(cmd_builder.get(), state.window->window_width, state.window->window_height);

            int wait_status = -100;

            // Submit, present, and wait for the presenting
            cmd_builder->present(&wait_status);

            state.graphics_driver->submit_command_list(*cmd_list);
            state.graphics_driver->wait_for(&wait_status);

            // Recreate the list and builder
            cmd_list = state.graphics_driver->new_command_list();
            cmd_builder = state.graphics_driver->new_command_builder(cmd_list.get());
        }

        result = ui_thread_deinitialization(state);

        if (result != 0) {
            LOG_ERROR(FRONTEND_CMDLINE, "UI thread deinitialization failed with code {}", result);
            return;
        }
    }

    void os_thread(emulator &state) {
        eka2l1::common::set_thread_name(os_thread_name);
        //state.graphics_sema.wait();

        while (!state.should_emu_quit) {
#if !defined(NDEBUG)
            try {
#endif
            state.symsys->loop();
#if !defined(NDEBUG)
            } catch (std::exception &exc) {
                LOG_ERROR(FRONTEND_CMDLINE, "Main loop exited with exception: ", exc.what());
                state.should_emu_quit = true;
                break;
            }
#endif
            if (state.should_emu_pause) {
                state.symsys->pause();
                state.should_graphics_pause = true;
                state.pause_sema.wait();
                state.symsys->unpause();
            }
        }

        state.symsys.reset();
        //state.graphics_sema.notify();
    }

    bool emulator_entry(emulator &state) {
        state.stage_one();

        const bool result = state.stage_two();

        // Instantiate UI and High-level interface threads
        ui_thread_obj = std::make_unique<std::thread>(ui_thread, std::ref(state));
        if (result)
            os_thread_obj = std::make_unique<std::thread>(os_thread, std::ref(state));

        // Run graphics driver on main entry.
        gr_thread_obj = std::make_unique<std::thread>(graphics_driver_thread, std::ref(state));

        return result;
    }

    void init_threads(emulator &state) {
        // Continue graphics initialization
        state.graphics_sema.notify();
    }

    void start_threads(emulator &state) {
        state.should_graphics_pause = false;
        state.should_emu_pause = false;
        state.pause_graphics_sema.notify();
    }

    void pause_threads(emulator &state) {
        state.should_emu_pause = true;
    }

    void press_key(emulator &state, int key, int key_state) {
        eka2l1::drivers::input_event evt;
        evt.type_ = eka2l1::drivers::input_event_type::key_raw;
        evt.key_.state_ = static_cast<eka2l1::drivers::key_state>(key_state);
        evt.key_.code_ = key;
        state.winserv->queue_input_from_driver(evt);
    }

    void touch_screen(emulator &state, int x, int y, int action) {
        eka2l1::drivers::input_event evt;
        evt.type_ = eka2l1::drivers::input_event_type::touch;
        evt.mouse_.pos_x_ = static_cast<int>(x);
        evt.mouse_.pos_y_ = static_cast<int>(y);
        evt.mouse_.button_ = eka2l1::drivers::mouse_button::mouse_button_left;
        evt.mouse_.action_ = static_cast<eka2l1::drivers::mouse_action>(action);
        state.winserv->queue_input_from_driver(evt);
    }
}