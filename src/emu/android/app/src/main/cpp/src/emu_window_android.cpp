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

#include <android/emu_window_android.h>
#include <common/log.h>
#include <common/raw_bind.h>
#include <glad/glad.h>

namespace eka2l1 {
    namespace drivers {
        emu_window_android::emu_window_android()
            : userdata(nullptr) {
        }

        bool emu_window_android::get_mouse_button_hold(const int mouse_btt) {
            return false;
        }

        vec2 emu_window_android::window_size() {
            return eka2l1::vec2(0, 0);
        }

        vec2 emu_window_android::window_fb_size() {
            return fb_size;
        }

        vec2d emu_window_android::get_mouse_pos() {
            return eka2l1::vec2d{ 0.0, 0.0 };
        }

        bool emu_window_android::set_cursor(cursor *cur) {
            return false;
        }

        void emu_window_android::cursor_visiblity(const bool visi) {
        }

        bool emu_window_android::cursor_visiblity() {
            return false;
        }

        bool emu_window_android::should_quit() {
            return false;
        }

        void emu_window_android::init(std::string title, vec2 size, const std::uint32_t flags) {
        }

        void emu_window_android::set_fullscreen(const bool is_fullscreen) {
        }

        void emu_window_android::change_title(std::string new_title) {
        }

        void emu_window_android::make_current() {
        }

        void emu_window_android::done_current() {
        }

        void emu_window_android::swap_buffer() {
        }

        void emu_window_android::shutdown() {
        }

        void emu_window_android::poll_events() {
        }

        void emu_window_android::set_userdata(void *new_userdata) {
            userdata = new_userdata;
        }

        void *emu_window_android::get_userdata() {
            return userdata;
        }

        void emu_window_android::surface_changed(ANativeWindow *surf, int width, int height) {
            if ((width > 0) && (height > 0)) {
                fb_size.x = width;
                fb_size.y = height;
            }
            surface_change_hook(surf);
        }
    }
}
