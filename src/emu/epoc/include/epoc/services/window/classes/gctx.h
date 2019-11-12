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
#include <drivers/graphics/common.h>
#include <drivers/graphics/graphics.h>

#include <common/linked.h>
#include <common/rgb.h>

#include <queue>
#include <string>

namespace eka2l1 {
    struct fbsfont;
}

namespace eka2l1::epoc {
    enum class brush_style {
        null = 0,
        solid = 1,
        pattern = 2,
        vertical_hatch = 3,
        foward_diagonal_hatch = 4,
        horizontal_hatch = 5,
        rearward_diagonal_hatch = 6,
        square_cross_hatch = 7,
        diamond_cross_hatch = 8
    };

    enum class pen_style {
        null = 0,
        solid = 1,
        dotted = 2,
        dashed = 3,
        dot_dash = 4,
        dot_dot_dash = 5
    };

    struct graphic_context : public window_client_obj {
        window_user *attached_window;
        std::unique_ptr<drivers::graphics_command_list> cmd_list;
        std::unique_ptr<drivers::graphics_command_list_builder> cmd_builder;

        common::double_linked_queue_element context_attach_link;
        fbsfont *text_font;

        bool recording{ false };

        brush_style fill_mode;
        pen_style line_mode;

        common::rgb brush_color;
        common::rgb pen_color;

        eka2l1::vec2 pen_size;

        void flush_queue_to_driver();

        enum class set_color_type {
            brush,
            pen
        };

        void reset_context();

        void do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left,
            eka2l1::vec2 bottom_right, std::u16string text);
        void do_command_draw_bitmap(service::ipc_context &ctx, drivers::handle h, 
            const eka2l1::rect &dest_rect);
        bool do_command_set_color(const set_color_type to_set);

        void active(service::ipc_context &context, ws_cmd cmd);
        void deactive(service::ipc_context &context, ws_cmd &cmd);
        void draw_bitmap(service::ipc_context &context, ws_cmd &cmd);
        void set_brush_color(service::ipc_context &context, ws_cmd &cmd);
        void set_brush_style(service::ipc_context &context, ws_cmd &cmd);
        void set_pen_style(service::ipc_context &context, ws_cmd &cmd);
        void draw_rect(service::ipc_context &context, ws_cmd &cmd);

        void reset(service::ipc_context &context, ws_cmd &cmd);
        void use_font(service::ipc_context &context, ws_cmd &cmd);

        void execute_command(service::ipc_context &context, ws_cmd &cmd) override;

        explicit graphic_context(window_server_client_ptr client, epoc::window *attach_win = nullptr);
    };
}
