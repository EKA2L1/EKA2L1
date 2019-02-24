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

#include <common/log.h>

namespace eka2l1::epoc {
    void window_user::queue_event(const epoc::event &evt) {
        if (!is_visible()) {
            LOG_TRACE("The window 0x{:X} is not visible, and can't receive any events",
                id);
            return;
        }

        window::queue_event(evt);
    }

    void window_user::priority_updated() {
        get_group()->get_driver()->set_window_priority(driver_win_id, redraw_priority());
    }

    void window_user::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        bool result = execute_command_for_general_node(ctx, cmd);

        if (result) {
            return;
        }

        TWsWindowOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsWinOpSetExtent: {
            ws_cmd_set_extent *extent = reinterpret_cast<decltype(extent)>(cmd.data_ptr);

            pos = extent->pos;
            size = extent->size;

            // Set position to the driver
            get_group()->get_driver()->set_window_size(driver_win_id, size);
            get_group()->get_driver()->set_window_pos(driver_win_id, pos);

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpSetVisible: {
            const bool op = *reinterpret_cast<bool *>(cmd.data_ptr);

            set_visible(op);
            get_group()->get_driver()->set_window_visible(driver_win_id, op);

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpSetShadowHeight: {
            shadow_height = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpShadowDisabled: {
            flags &= ~shadow_disable;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= shadow_disable;
            }

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpSetBackgroundColor: {
            if (cmd.header.cmd_len == 0) {
                clear_color = -1;
                ctx.set_request_status(KErrNone);

                break;
            }

            clear_color = *reinterpret_cast<int *>(cmd.data_ptr);
            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsWinOpPointerFilter: {
            LOG_TRACE("Filtering pointer event type");

            ws_cmd_pointer_filter *filter_info = reinterpret_cast<ws_cmd_pointer_filter *>(cmd.data_ptr);
            filter &= ~filter_info->mask;
            filter |= filter_info->flags;

            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsWinOpSetPointerGrab: {
            flags &= ~allow_pointer_grab;

            if (*reinterpret_cast<bool *>(cmd.data_ptr)) {
                flags |= allow_pointer_grab;
            }

            ctx.set_request_status(KErrNone);
            break;
        }

        case EWsWinOpActivate: {
            flags |= active;

            // When a window actives, a redraw is needed
            // Redraw happens with all of the screen
            client->queue_redraw(this);
            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpInvalidate: {
            LOG_INFO("Invalidate stubbed, currently we redraws all the screen");
            irect = *reinterpret_cast<invalidate_rect *>(cmd.data_ptr);

            // Invalidate needs redraw
            redraw_evt_id = client->queue_redraw(this);

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpBeginRedraw: {
            for (auto &context : contexts) {
                context->recording = true;

                while (!context->draw_queue.empty()) {
                    context->draw_queue.pop();
                }
            }

            LOG_TRACE("Begin redraw!");

            // Cancel pending redraw event, since by using this,
            // we already starts one
            if (redraw_evt_id) {
                client->deque_redraw(redraw_evt_id);
                redraw_evt_id = 0;
            }

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsWinOpEndRedraw: {
            for (auto &context : contexts) {
                context->recording = false;
                context->flush_queue_to_driver();
            }

            LOG_TRACE("End redraw!");

            ctx.set_request_status(KErrNone);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented window user opcode 0x{:X}!", cmd.header.op);
            break;
        }
        }
    }
}