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

#include <drivers/graphics/graphics.h>
#include <services/window/classes/winbase.h>

#include <vector>

namespace eka2l1::epoc {
    struct window_top_user;
    using message_data = std::vector<std::uint8_t>;

    struct window_group : public epoc::window {
        std::u16string name;
        std::unique_ptr<window_top_user> top;
        std::queue<message_data> msg_datas;

        bool can_receive_focus() {
            return flags & flag_focus_receiveable;
        }

        void set_receive_focus(const bool val) {
            flags &= ~flag_focus_receiveable;

            if (val)
                flags |= flag_focus_receiveable;
        }

        explicit window_group(window_server_client_ptr client, screen *scr, epoc::window *parent,
            const std::uint32_t client_handle);

        // ===================== COMMAND OPCODES =======================
        void set_text_cursor(service::ipc_context &context, ws_cmd &cmd);
        void receive_focus(service::ipc_context &context, ws_cmd &cmd);
        void add_priority_key(service::ipc_context &context, ws_cmd &cmd);
        void execute_command(service::ipc_context &context, ws_cmd &cmd) override;

        eka2l1::vec2 get_origin() override;

        void lost_focus();
        void gain_focus();

        void queue_message_data(const std::uint8_t *data, const std::size_t data_size);
        void get_message_data(std::uint8_t *data, std::size_t &dest_size);
    };
}