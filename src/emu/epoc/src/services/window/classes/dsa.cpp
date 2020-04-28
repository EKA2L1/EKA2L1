/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#include <epoc/services/window/classes/dsa.h>
#include <epoc/services/window/classes/winuser.h>
#include <epoc/services/window/op.h>
#include <epoc/services/window/window.h>

#include <epoc/utils/consts.h>
#include <epoc/utils/err.h>

#include <epoc/kernel.h>

namespace eka2l1::epoc {
    dsa::dsa(window_server_client_ptr client)
        : window_client_obj(client, nullptr)
        , husband_(nullptr)
        , state_(state_none) {
        kernel_system *kern = client->get_ws().get_kernel_system();

        // Each message is an integer. Allow maximum of 10 messages
        dsa_must_abort_queue_ = kern->create<kernel::msg_queue>("DsaAbortQueue", 4, 10);
        dsa_complete_queue_ = kern->create<kernel::msg_queue>("DsaCompleteQueue", 4, 10);
    };

    dsa::~dsa() {
        if (husband_) {
            husband_->set_dsa_active(false);
            husband_->direct = nullptr;
        }
    }

    void dsa::request_access(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        if (state_ != state_none) {
            if (state_ != state_completed) {
                LOG_ERROR("Requesting access on a DSA in progress");
                ctx.set_request_status(epoc::error_argument);
                return;
            } else {
                state_ = state_none;
            }
        }

        std::uint32_t window_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(client->get_object(window_handle));

        // what the fuck msvc
        if ((!user) || (user->type != epoc::window_kind::client)) {
            LOG_ERROR("Invalid window handle given 0x{:X}", window_handle);
            ctx.set_request_status(epoc::error_argument);
            return;
        }

        husband_ = user;

        // We allow the whole window region to be DSAed!
        // But what is the point... To override a DSA? Should that be possible...
        // TODO: Verify...
        if (husband_->is_dsa_active()) {
            ctx.set_request_status(0);
            return;
        }

        LOG_TRACE("DSA requested for window {}", user->id);

        state_ = state_prepare;
        husband_->set_dsa_active(true);
        husband_->direct = this;

        eka2l1::rect extent;
        extent.top = husband_->pos;
        extent.size = husband_->size;

        husband_->scr->dsa_rect.merge(extent);

        ctx.set_request_status(1);
    }

    void dsa::get_region(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        std::uint32_t max_rects = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

        if ((max_rects == 0) || (state_ != state_prepare)) {
            // What? Nothing?
            ctx.set_request_status(epoc::CINT32_MAX);
            return;
        }

        // The whole window for you!
        eka2l1::rect extent;
        extent.top = husband_->pos;
        extent.size = husband_->size;

        state_ = state_running;

        ctx.write_arg_pkg<eka2l1::rect>(reply_slot, extent);
        ctx.set_request_status(1);
    }

    void dsa::cancel(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        state_ = state_completed;
        ctx.set_request_status(epoc::error_none);
    }

    void dsa::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        ws_dsa_op op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case ws_dsa_get_send_queue:
        case ws_dsa_get_rec_queue: {
            kernel_system *kern = client->get_ws().get_kernel_system();
            std::int32_t target_handle = 0;

            target_handle = kern->open_handle_with_thread(ctx.msg->own_thr, (op == ws_dsa_get_send_queue) ? 
                dsa_must_abort_queue_ : dsa_complete_queue_, kernel::owner_type::process);

            ctx.set_request_status(target_handle);
            break;
        }

        case ws_dsa_request:
            request_access(ctx, cmd);
            break;

        case ws_dsa_get_region:
            get_region(ctx, cmd);
            break;

        case ws_dsa_cancel:
            cancel(ctx, cmd);
            break;

        default: {
            LOG_ERROR("Unimplemented DSA opcode {}", cmd.header.op);
            break;
        }
        }
    }
}
