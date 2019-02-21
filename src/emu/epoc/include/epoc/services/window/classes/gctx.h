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

#include <epoc/services/window/classes/winuser.h>

#include <string>
#include <queue>

namespace eka2l1::epoc {
    struct draw_command {
        int gc_command;
        std::string buf;

        template <typename T>
        T internalize() {
            T d = *reinterpret_cast<T *>(&(buf[0]));
            buf.erase(0, sizeof(T));

            return d;
        }

        template <typename T>
        void externalize(const T &v) {
            buf.append(reinterpret_cast<const char *>(&v), sizeof(T));
        }
    };

    struct graphic_context : public window_client_obj {
        std::shared_ptr<window_user> attached_window;
        std::queue<draw_command> draw_queue;

        bool recording{ false };

        void flush_queue_to_driver();

        void do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left, 
            eka2l1::vec2 bottom_right, std::u16string text);

        void active(service::ipc_context &context, ws_cmd cmd);
        void execute_command(service::ipc_context &context, ws_cmd cmd) override;

        explicit graphic_context(window_server_client_ptr client, screen_device_ptr scr = nullptr,
            window_ptr win = nullptr);
    };
}