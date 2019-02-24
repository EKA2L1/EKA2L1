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

namespace eka2l1::epoc {
    void graphic_context::active(service::ipc_context &context, ws_cmd cmd) {
        const std::uint32_t window_to_attach_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        attached_window = std::reinterpret_pointer_cast<epoc::window_user>(client->get_object(window_to_attach_handle));

        // Attach context with window
        attached_window->contexts.push_back(this);

        // Afaik that the pointer to CWsScreenDevice is internal, so not so scared of general users touching
        // this.
        context.set_request_status(attached_window->dvc->id);
    }

    void graphic_context::do_command_draw_text(service::ipc_context &ctx, eka2l1::vec2 top_left, eka2l1::vec2 bottom_right, std::u16string text) {
        if (attached_window->cursor_pos == vec2(-1, -1)) {
            LOG_TRACE("Cursor position not set, not drawing the text");
            return;
        }

        draw_command command;
        command.gc_command = EWsGcOpDrawTextPtr;

        auto pos_to_screen = attached_window->cursor_pos + top_left;
        command.externalize(pos_to_screen);

        if (bottom_right == vec2(-1, -1)) {
            command.externalize(bottom_right);
        } else {
            pos_to_screen = bottom_right + attached_window->cursor_pos;
            command.externalize(pos_to_screen);
        }

        std::uint32_t text_len = static_cast<std::uint32_t>(text.length());

        // LOG_TRACE("Drawing to window text {}", common::ucs2_to_utf8(text));

        command.externalize(text_len);
        command.buf.append(reinterpret_cast<char *>(&text[0]), text_len * sizeof(char16_t));

        draw_queue.push(command);

        ctx.set_request_status(KErrNone);
    }

    void graphic_context::flush_queue_to_driver() {
        // Flushing
        epoc::window_group_ptr group = std::reinterpret_pointer_cast<epoc::window_group>(
            attached_window->parent);

        eka2l1::graphics_driver_client_ptr driver = group->dvc->driver;

        // Since we are sending multiple opcodes, lock the driver first
        // That way, all opcodes from this context should be processed in only one take
        driver->lock_driver_from_process();

        rect inv_rect = rect{ attached_window->irect.in_top_left,
            attached_window->irect.in_bottom_right - attached_window->irect.in_top_left };

        // There should be an invalidate window
        driver->invalidate(inv_rect);

        attached_window->irect.in_top_left = vec2(0, 0);
        attached_window->irect.in_bottom_right = vec2(0, 0);

        while (!draw_queue.empty()) {
            auto draw_command = std::move(draw_queue.front());

            switch (draw_command.gc_command) {
            case EWsGcOpDrawTextPtr: {
                // Deopt and send
                eka2l1::vec2 top_left = draw_command.internalize<vec2>();
                eka2l1::vec2 bottom_right = draw_command.internalize<vec2>();

                eka2l1::rect r;
                r.top = top_left;

                if (bottom_right == vec2(-1, -1)) {
                    r.size = vec2(-1, -1);
                } else {
                    r.size = bottom_right - top_left;
                }

                std::uint32_t text_len = draw_command.internalize<std::uint32_t>();

                std::u16string text(reinterpret_cast<char16_t *>(&draw_command.buf[0]), text_len);
                driver->draw_text(r, common::ucs2_to_utf8(text));

                break;
            }

            default: {
                LOG_TRACE("Can't transform IR opcode to driver opcode: 0x{:x}", draw_command.gc_command);
                break;
            }
            }

            draw_queue.pop();
        }

        driver->end_invalidate();
        driver->unlock_driver_from_process();
    }

    void graphic_context::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        TWsGcOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsGcOpActivate: {
            active(ctx, cmd);
            break;
        }

        // Brush is fill, pen is outline
        case EWsGcOpSetBrushColor: {
            std::string buf;
            buf.resize(sizeof(int));

            std::memcpy(&buf[0], cmd.data_ptr, sizeof(int));

            draw_command command{ EWsGcOpSetBrushColor, buf };
            draw_queue.push(command);

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsGcOpSetBrushStyle: {
            LOG_ERROR("SetBrushStyle not supported, stub");
            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsGcOpSetPenStyle: {
            LOG_ERROR("Pen operation not supported yet (wait for ImGui support outline color)");
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsGcOpSetPenColor: {
            LOG_ERROR("Pen operation not supported yet (wait for ImGui support outline color)");
            ctx.set_request_status(KErrNone);
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
            auto this_ctx = std::find(attached_window->contexts.begin(), attached_window->contexts.end(),
                this);

            if (this_ctx != attached_window->contexts.end()) {
                attached_window->contexts.erase(this_ctx);
            }

            attached_window.reset();
            ctx.set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_WARN("Unimplemented opcode for graphics context operation: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }

    graphic_context::graphic_context(window_server_client_ptr client, screen_device_ptr scr,
        window_ptr win)
        : window_client_obj(client)
        , attached_window(std::reinterpret_pointer_cast<window_user>(win)) {
    }
}