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
#include <common/e32inc.h>
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

    void graphic_context::do_command_set_color(service::ipc_context &ctx, const void *data, const set_color_type to_set) {
        const eka2l1::vecx<int, 4> color = common::rgb_to_vec(*reinterpret_cast<const common::rgb*>(data));
        
        switch (to_set) {
        case set_color_type::brush: {
            cmd_builder->set_brush_color({ color[0], color[1], color[2] });
            break;
        }

        default: {
            LOG_ERROR("Unhandle support for setting color for type {}", static_cast<int>(to_set));
            break;
        }
        }

        ctx.set_request_status(epoc::error_none);
    }

    void graphic_context::flush_queue_to_driver() {
        drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();

        // Unbind current bitmap
        cmd_builder->bind_bitmap(0);
        driver->submit_command_list(*cmd_list);
    }

    void graphic_context::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        TWsGcOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsGcOpActivate: {
            active(ctx, cmd);
            break;
        }

        // Brush is fill, pen is outline
        case EWsGcOpSetBrushColor: {
            do_command_set_color(ctx, cmd.data_ptr, set_color_type::brush);
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

            if ((ctx.sys->get_symbian_version_use() <= epocver::epoc93) && (cmd.header.cmd_len <= 8)) {
                draw_text = *ctx.get_arg<std::u16string>(remote_slot);
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
            if ((ctx.sys->get_symbian_version_use() <= epocver::epoc93) && (cmd.header.cmd_len <= 16)) {
                draw_text = *ctx.get_arg<std::u16string>(remote_slot);
            } else {
                epoc::desc16 *text_des = draw_text_info->text.get(ctx.msg->own_thr->owning_process());
                std::u16string draw_text = text_des->to_std_string(ctx.msg->own_thr->owning_process());
            }

            // TODO: align
            do_command_draw_text(ctx, draw_text_info->left_top_pos, draw_text_info->right_bottom_pos, draw_text);

            break;
        }

        case EWsGcOpDeactivate: {
            context_attach_link.deque();

            // Might have to flush sooner, since this window can be used with another
            // TODO pent0: This may gone insane
            flush_queue_to_driver();

            attached_window = nullptr;
            ctx.set_request_status(epoc::error_none);

            break;
        }

        case EWsGcOpDrawBitmap: {
            ws_cmd_draw_bitmap *bitmap_cmd = reinterpret_cast<ws_cmd_draw_bitmap *>(cmd.data_ptr);
            epoc::bitwise_bitmap *bw_bmp = client->get_ws().get_bitmap(bitmap_cmd->handle);

            if (!bw_bmp) {
                ctx.set_request_status(epoc::error_argument);
                break;
            }

            drivers::graphics_driver *driver = client->get_ws().get_graphics_driver();
            epoc::bitmap_cache *cacher = client->get_ws().get_bitmap_cache();
            drivers::handle bmp_driver_handle = cacher->add_or_get(driver, cmd_builder.get(), bw_bmp);

            do_command_draw_bitmap(ctx, bmp_driver_handle, rect(bitmap_cmd->pos, bw_bmp->header_.size_pixels));

            break;
        }

        default: {
            LOG_WARN("Unimplemented opcode for graphics context operation: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }

    graphic_context::graphic_context(window_server_client_ptr client, epoc::window *attach_win)
        : window_client_obj(client, nullptr)
        , attached_window(reinterpret_cast<epoc::window_user*>(attach_win)) {
    }
}
