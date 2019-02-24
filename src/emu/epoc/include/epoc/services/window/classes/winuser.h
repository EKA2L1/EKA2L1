/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/window/classes/winbase.h>
#include <epoc/services/window/common.h>

namespace eka2l1::epoc {
    struct graphic_context;

    struct window_group;
    using window_group_ptr = std::shared_ptr<epoc::window_group>;

    struct window_user : public epoc::window {
        epoc::display_mode dmode;
        epoc::window_type win_type;

        // The position
        eka2l1::vec2 pos{ 0, 0 };
        eka2l1::vec2 size{ 0, 0 };

        std::vector<epoc::graphic_context *> contexts;

        std::uint32_t clear_color = 0xFFFFFFFF;
        std::uint32_t filter = pointer_filter_type::all;

        eka2l1::vec2 cursor_pos{ -1, -1 };

        struct invalidate_rect {
            vec2 in_top_left;
            vec2 in_bottom_right;
        } irect;

        std::uint32_t redraw_evt_id;
        std::uint32_t driver_win_id{ 0 };

        void priority_updated() override;

        window_user(window_server_client_ptr client, screen_device_ptr dvc,
            epoc::window_type type_of_window, epoc::display_mode dmode)
            : window(client, dvc, window_kind::client)
            , win_type(type_of_window)
            , dmode(dmode) {
        }

        int shadow_height{ 0 };

        enum {
            shadow_disable = 0x1000,
            active = 0x2000,
            visible = 0x4000,
            allow_pointer_grab = 0x8000
        };

        std::uint32_t flags;

        bool is_visible() {
            return flags & visible;
        }

        void set_visible(bool vis) {
            flags &= ~visible;

            if (vis) {
                flags |= visible;
            }
        }

        void queue_event(const epoc::event &evt) override;
        void execute_command(service::ipc_context &context, ws_cmd cmd) override;

        epoc::window_group_ptr get_group() {
            return std::reinterpret_pointer_cast<epoc::window_group>(parent);
        }
    };
}