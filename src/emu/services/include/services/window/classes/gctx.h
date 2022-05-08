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

#include <drivers/graphics/common.h>
#include <drivers/graphics/graphics.h>
#include <services/window/classes/winuser.h>

#include <common/linked.h>
#include <common/region.h>
#include <common/rgb.h>
#include <common/vecx.h>

#include <queue>
#include <string>

namespace eka2l1 {
    struct fbsfont;
}

namespace eka2l1::epoc {
    struct bitwise_bitmap;

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
        canvas_base *attached_window;

        common::double_linked_queue_element context_attach_link;
        fbsfont *text_font;

        bool recording{ false };
        bool flushed{ false };

        brush_style fill_mode;
        pen_style line_mode;

        common::rgba brush_color;
        common::rgba pen_color;

        eka2l1::vec2 pen_size;
        eka2l1::rect clipping_rect;
        common::region clipping_region;

        void submit_queue_commands(kernel::thread *rq);
        void on_command_batch_done(service::ipc_context &ctx) override;

        enum class set_color_type {
            brush,
            pen
        };

        void reset_context();
        bool no_building() const;

        void do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left,
            eka2l1::vec2 bottom_right, const std::u16string &text, epoc::text_alignment align,
            const int baseline_offset, const int margin, const bool fill_surrounding);

        void do_command_draw_bitmap(service::ipc_context &ctx, void *bitmap, eka2l1::rect source_rect, eka2l1::rect dest_rect, const std::uint8_t flags);
        bool get_brush_color(eka2l1::vec4 &color_brush);
        bool get_pen_color_and_style(eka2l1::vec4 &pen_color, drivers::pen_style &style);

        void do_submit_clipping();

        void draw_mask_impl(void *source_bitmap, void *mask_bitmap, eka2l1::rect dest_rect, eka2l1::rect source_rect, const std::uint8_t flags);

        void active(service::ipc_context &context, ws_cmd cmd);
        void deactive(service::ipc_context &context, ws_cmd &cmd);
        void draw_bitmap(service::ipc_context &context, ws_cmd &cmd);
        void draw_bitmap_2(service::ipc_context &context, ws_cmd &cmd);
        void draw_bitmap_3(service::ipc_context &context, ws_cmd &cmd);
        void ws_draw_bitmap_masked(service::ipc_context &context, ws_cmd &cmd);
        void set_brush_color(service::ipc_context &context, ws_cmd &cmd);
        void set_brush_style(service::ipc_context &context, ws_cmd &cmd);
        void set_pen_color(service::ipc_context &context, ws_cmd &cmd);
        void set_pen_style(service::ipc_context &context, ws_cmd &cmd);
        void set_pen_size(service::ipc_context &context, ws_cmd &cmd);
        void draw_line(service::ipc_context &context, ws_cmd &cmd);
        void draw_rect(service::ipc_context &context, ws_cmd &cmd);
        void clear(service::ipc_context &context, ws_cmd &cmd);
        void clear_rect(service::ipc_context &context, ws_cmd &cmd);
        void draw_text(service::ipc_context &context, ws_cmd &cmd);
        void draw_box_text_optimised1(service::ipc_context &context, ws_cmd &cmd);
        void draw_box_text_optimised2(service::ipc_context &context, ws_cmd &cmd);
        void plot(service::ipc_context &context, ws_cmd &cmd);
        void set_underline_style(service::ipc_context &context, ws_cmd &cmd);
        void set_strikethrough_style(service::ipc_context &context, ws_cmd &cmd);
        void set_draw_mode(service::ipc_context &context, ws_cmd &cmd);

        void gdi_blt_impl(service::ipc_context &context, ws_cmd &cmd, const int ver, const bool ws);
        void gdi_blt_masked(service::ipc_context &context, ws_cmd &cmd);
        void gdi_blt2(service::ipc_context &context, ws_cmd &cmd);
        void gdi_blt3(service::ipc_context &context, ws_cmd &cmd);
        void gdi_ws_blt2(service::ipc_context &context, ws_cmd &cmd);
        void gdi_ws_blt3(service::ipc_context &context, ws_cmd &cmd);
        void gdi_ws_blt_masked(service::ipc_context &context, ws_cmd &cmd);

        void use_font(service::ipc_context &context, ws_cmd &cmd);
        void discard_font(service::ipc_context &context, ws_cmd &cmd);
        void reset(service::ipc_context &context, ws_cmd &cmd);
        void destroy(service::ipc_context &context, ws_cmd &cmd);
        void set_clipping_rect(service::ipc_context &context, ws_cmd &cmd);
        void cancel_clipping_rect(service::ipc_context &context, ws_cmd &cmd);
        void set_opaque(service::ipc_context &context, ws_cmd &cmd);

        bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;

        explicit graphic_context(window_server_client_ptr client, epoc::window *attach_win = nullptr);
        ~graphic_context() override;
    };
}
