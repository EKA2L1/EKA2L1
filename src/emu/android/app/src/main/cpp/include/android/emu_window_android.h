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
#pragma once

#include <common/vecx.h>
#include <drivers/graphics/emu_window.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/native_window_jni.h>

#include <any>
#include <functional>

namespace eka2l1 {
    namespace drivers {
        class emu_window_android : public emu_window {
            vec2 emu_screen_size;
            vec2 pos_with_title;

            ANativeWindow* render_window;
            ANativeWindow* host_window;
            EGLConfig egl_config;
            EGLSurface egl_surface;
            EGLContext egl_context;
            EGLDisplay egl_display;

            void *userdata;
            bool is_fullscreen_now;
            bool inited;

        public:
            int window_width;
            int window_height;

            explicit emu_window_android();

            void surface_changed(ANativeWindow *surf, int width, int height);
            void init_gl();
            void init_surface();
            void create_surface();
            void destroy_surface();

            bool get_mouse_button_hold(const int mouse_btt) override;

            void change_title(std::string new_title) override;

            void init(std::string title, vec2 size, const std::uint32_t flags) override;
            void make_current() override;
            void done_current() override;
            void swap_buffer() override;
            void poll_events() override;
            void set_userdata(void *userdata) override;
            void *get_userdata() override;
            void set_fullscreen(const bool is_fullscreen) override;

            bool should_quit() override;

            void shutdown() override;

            vec2 window_size() override;
            vec2 window_fb_size() override;
            vec2d get_mouse_pos() override;
            bool set_cursor(cursor *cur) override;
            void cursor_visiblity(const bool visi) override;
            bool cursor_visiblity() override;
        };
    }
}
