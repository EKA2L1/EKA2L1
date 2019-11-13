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

#include <epoc/services/window/classes/gctx.h>
#include <epoc/services/window/classes/scrdvc.h>
#include <epoc/services/window/classes/wingroup.h>
#include <epoc/services/window/classes/winuser.h>
#include <epoc/services/window/op.h>
#include <epoc/services/window/opheader.h>
#include <epoc/services/window/window.h>

#include <epoc/epoc.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/rgb.h>

#include <epoc/utils/err.h>

namespace eka2l1::epoc {
    void graphic_context::active(service::ipc_context &context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = reinterpret_cast<epoc::window_user*>(client->get_object(window_to_attach_handle));

        // Attach context with window
        attached_window->attached_contexts.push(&context_attach_link);

        // Afaik that the pointer to CWsScreenDevice is internal, so not so scared of general users touching
        // this.
        context.set_request_status(attached_window->scr->number);

        drivers::graphics_driver *drv = client->get_ws().get_graphics_driver();

        // Make new command list
        cmd_list = drv->new_command_list();
        cmd_builder = drv->new_command_builder(cmd_list.get());

        // Add first command list, binding our window bitmap
        if (attached_window->driver_win_id == 0) {
            attached_window->driver_win_id = drivers::create_bitmap(drv, attached_window->size);
            attached_window->resize_needed = false;
        } else if (attached_window->resize_needed) {
            // Try to resize our bitmap. My NVIDIA did forgive me if texture has same spec
            // as before, but not Intel...
            cmd_builder->resize_bitmap(attached_window->driver_win_id, attached_window->size);
        }

        cmd_builder->bind_bitmap(attached_window->driver_win_id);
    }

    void graphic_context::do_command_draw_bitmap(service::ipc_context &ctx, drivers::handle h,
        const eka2l1::rect &dest_rect) {
        eka2l1::rect source_rect;
        source_rect.top = { 0, 0 };
        source_rect.size = dest_rect.size;

        cmd_builder->draw_bitmap(h, dest_rect.top, source_rect, false);
        ctx.set_request_status(epoc::error_none);
    }

    void graphic_context::do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left, eka2l1::vec2 bottom_right, std::u16string text) {
        if (attached_window->cursor_pos == vec2(-1, -1)) {
            LOG_TRACE("Cursor position not set, not drawing the text");
            return;
        }

        // TODO: Get text data from the font. Maybe upload them as atlas.

        ctx.set_request_status(epoc::error_none);
    }

    bool graphic_context::do_command_set_color(const set_color_type to_set) {
        eka2l1::vecx<int, 4> color;
        
        switch (to_set) {
        case set_color_type::brush: {
            // Don't bother even sending any draw command
            switch (fill_mode) {
            case brush_style::null:
                return false;

            case brush_style::solid:
                color = common::rgb_to_vec(brush_color);
                cmd_builder->set_brush_color({ color[1], color[2], color[3] });
                break;

            default:
                LOG_WARN("Unhandled brush style {}", static_cast<std::int32_t>(fill_mode));
                break;
            }

            break;
        }

        case set_color_type::pen: {
            // Don't bother even sending any draw command
            switch (line_mode) {
            case pen_style::null:
                return false;

            case pen_style::solid:
                color = common::rgb_to_vec(pen_color);
                cmd_builder->set_brush_color({ color[1], color[2], color[3] });
                break;

            default:
                LOG_WARN("Unhandled pen style {}", static_cast<std::int32_t>(fill_mode));
                break;
            }

            break;
        }

        default: {
            LOG_ERROR("Unhandle support for setting color for type {}", static_cast<int>(to_set));
            break;
        }
        }

        return true;
    }

    void graphic_context::flush_queue_to_driver() {
        drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();

        // Unbind current bitmap
        cmd_builder->bind_bitmap(0);
        driver->submit_command_list(*cmd_list);
    }

    void graphic_context::set_brush_color(service::ipc_context &context, ws_cmd &cmd) {
        brush_color = *reinterpret_cast<const common::rgb*>(cmd.data_ptr);
        context.set_request_status(epoc::error_none);
    }

    void graphic_context::deactive(service::ipc_context &context, ws_cmd &cmd) {        
        context_attach_link.deque();

        // Might have to flush sooner, since this window can be used with another
        // TODO pent0: This may gone insane
        flush_queue_to_driver();

        attached_window = nullptr;
        context.set_request_status(epoc::error_none);
    }
    
    void graphic_context::draw_bitmap(service::ipc_context &context, ws_cmd &cmd) {
        ws_cmd_draw_bitmap *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap *>(cmd.data_ptr);
        epoc::bitwise_bitmap *bw_bmp = client->get_ws().get_bitmap(bitmap_cmd->handle);

        if (!bw_bmp) {
            context.set_request_status(epoc::error_argument);
            return;
        }

        drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();
        epoc::bitmap_cache *cacher = client->get_ws().get_bitmap_cache();
        drivers::handle bmp_driver_handle = cacher->add_or_get(driver, cmd_builder.get(), bw_bmp);

        do_command_draw_bitmap(context, bmp_driver_handle, rect(bitmap_cmd->pos, bw_bmp->header_.size_pixels));
    }
    
    void graphic_context::set_brush_style(service::ipc_context &context, ws_cmd &cmd) {
        fill_mode = *reinterpret_cast<brush_style*>(cmd.data_ptr);
        context.set_request_status(epoc::error_none);
    }
    
    void graphic_context::set_pen_style(service::ipc_context &context, ws_cmd &cmd) {
        line_mode = *reinterpret_cast<pen_style*>(cmd.data_ptr);
        context.set_request_status(epoc::error_none);
    }
    
    void graphic_context::draw_rect(service::ipc_context &context, ws_cmd &cmd) {
        eka2l1::rect area = *reinterpret_cast<eka2l1::rect*>(cmd.data_ptr);

        if (do_command_set_color(set_color_type::pen)) {
            // We want to draw the rectangle that backup the real rectangle, to create borders.
            eka2l1::rect backup_border = area;
            backup_border.top -= pen_size;
            backup_border.size += pen_size * 2;
            
            cmd_builder->draw_rectangle(backup_border);
        }

        // Draw the real rectangle! Hurray!
        if (do_command_set_color(set_color_type::brush)) {
            cmd_builder->draw_rectangle(area);
        }

        context.set_request_status(epoc::error_none);
    }
    
    void graphic_context::reset_context() {
        text_font = nullptr;
        
        fill_mode = brush_style::null;
        line_mode = pen_style::null;

        pen_size = { 1, 1 };
    }
    
    void graphic_context::reset(service::ipc_context &context, ws_cmd &cmd) {
        reset_context();
        context.set_request_status(epoc::error_none);
    }

    void graphic_context::use_font(service::ipc_context &context, ws_cmd &cmd) {
        std::uint32_t font_handle = *reinterpret_cast<std::uint32_t*>(cmd.data_ptr);
    }
    
    void graphic_context::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_graphics_context_opcode op = static_cast<decltype(op)>(cmd.header.op);

        // General rules: Stub to err_none = nullptr, implement = function pointer
        //                Do nothing = add nothing
        using ws_graphics_context_op_handler = std::function<void(graphic_context*, 
            service::ipc_context &ctx, ws_cmd &cmd)>;

        using ws_graphics_context_table_op =  std::map<ws_graphics_context_opcode, ws_graphics_context_op_handler>;

        static const ws_graphics_context_table_op v171u_opcode_handlers = {
            { ws_gc_u171_active, &graphic_context::active },
            { ws_gc_u171_set_brush_color, &graphic_context::set_brush_color },
            { ws_gc_u171_set_brush_style, &graphic_context::set_brush_style },
            { ws_gc_u171_set_pen_style, &graphic_context::set_pen_style },
            { ws_gc_u171_deactive, &graphic_context::deactive },
            { ws_gc_u171_reset, &graphic_context::reset },
            { ws_gc_u171_use_font, &graphic_context::use_font },
            { ws_gc_u171_draw_rect, &graphic_context::draw_rect },
            { ws_gc_u171_draw_bitmap, &graphic_context::draw_bitmap }
        };
        
        static const ws_graphics_context_table_op curr_opcode_handlers = {
            { ws_gc_curr_active, &graphic_context::active },
            { ws_gc_curr_set_brush_color, &graphic_context::set_brush_color },
            { ws_gc_curr_set_brush_style, &graphic_context::set_brush_style },
            { ws_gc_curr_set_pen_style, &graphic_context::set_pen_style },
            { ws_gc_curr_deactive, &graphic_context::deactive },
            { ws_gc_curr_reset, &graphic_context::reset },
            { ws_gc_curr_use_font, &graphic_context::use_font },
            { ws_gc_curr_draw_rect, &graphic_context::draw_rect },
            { ws_gc_curr_draw_bitmap, &graphic_context::draw_bitmap }
        };

        epoc::version cli_ver = client->client_version();
        ws_graphics_context_op_handler handler = nullptr;
        
        #define FIND_OPCODE(op, table)                                                  \
            auto result = table.find(op);                                               \
            if (result == table.end() || !result->second) {                             \
                LOG_WARN("Unimplemented graphics context opcode {}", cmd.header.op);    \
                return;                                                                 \
            }                                                                           \
            handler = result->second;

        if (cli_ver.major == 1 && cli_ver.minor == 0) {
            if (cli_ver.build <= 171) {
                // Execute table 1
                FIND_OPCODE(op, v171u_opcode_handlers) 
            } else {
                // Execute table 2
                FIND_OPCODE(op, curr_opcode_handlers)
            }
        }

        handler(this, ctx, cmd);
            /*
        case EWsGcOpActivate: {
            active(ctx, cmd);
            break;
        }

        // Brush is fill, pen is outline
        case EWsGcOpSetBrushColor: {
            break;
        }

        case EWsGcOpSetBrushStyle: {
            LOG_ERROR("SetBrushStyle not supported, stub");
            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsGcOpSetPenStyle: {
            LOG_ERROR("Pen operation not supported yet");
            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsGcOpSetPenColor: {
            LOG_ERROR("Pen operation not supported yet");
            ctx.set_request_status(epoc::error_none);
            break;
        }

        case EWsGcOpDrawTextPtr: {
            ws_cmd_draw_text_ptr *draw_text_info = reinterpret_cast<decltype(draw_text_info)>(cmd.data_ptr);

            std::u16string draw_text;

            if ((ctx.sys->get_symbian_version_use() <= epocver::epoc94) && (cmd.header.cmd_len <= 8)) {
                draw_text = *ctx.get_arg<std::u16string>(0);
            } else {
                epoc::desc16 *text_des = draw_text_info->text.get(ctx.msg->own_thr->owning_process());
                std::u16string draw_text = text_des->to_std_string(ctx.msg->own_thr->owning_process());
            }

            do_command_draw_text(ctx, draw_text_info->pos, vec2(-1, -1), draw_text);

            break;
        }

        case EWsGcOpDrawTextVertical: {
            ws_cmd_draw_text_vertical_v94 *draw_text_info = reinterpret_cast<decltype(draw_text_info)>(cmd.data_ptr);

            std::u16string text;
            std::uint32_t text_len = draw_text_info->length;

            char16_t *text_ptr = reinterpret_cast<char16_t *>(draw_text_info + 1);
            text.assign(text_ptr, text_len);

            do_command_draw_text(ctx, draw_text_info->pos, draw_text_info->bottom_right, text);

            break;
        }

        case EWsGcOpDrawBoxTextPtr: {
            ws_cmd_draw_box_text_ptr *draw_text_info = reinterpret_cast<decltype(draw_text_info)>(cmd.data_ptr);

            std::u16string draw_text;

            // on EPOC <= 9, the struct only contains the bound
            if ((ctx.sys->get_symbian_version_use() <= epocver::epoc94) && (cmd.header.cmd_len <= 16)) {
                draw_text = *ctx.get_arg<std::u16string>(0);
            } else {
                epoc::desc16 *text_des = draw_text_info->text.get(ctx.msg->own_thr->owning_process());
                std::u16string draw_text = text_des->to_std_string(ctx.msg->own_thr->owning_process());
            }

            // TODO: align
            do_command_draw_text(ctx, draw_text_info->left_top_pos, draw_text_info->right_bottom_pos, draw_text);

            break;
        }

        case EWsGcOpDeactivate: {
            break;
        }

        case EWsGcOpDrawBitmap: {
            
        }*/
    }

    graphic_context::graphic_context(window_server_client_ptr client, epoc::window *attach_win)
        : window_client_obj(client, nullptr)
        , attached_window(reinterpret_cast<epoc::window_user*>(attach_win))
        , text_font(nullptr)
        , fill_mode(brush_style::null)
        , line_mode(pen_style::null)
        , brush_color(0)
        , pen_color(0)
        , pen_size(1, 1) {
    }
}
