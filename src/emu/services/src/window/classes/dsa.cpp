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

#include <services/window/classes/dsa.h>
#include <services/window/classes/winuser.h>
#include <services/window/op.h>
#include <services/window/window.h>

#include <utils/consts.h>
#include <utils/err.h>

#include <kernel/kernel.h>

namespace eka2l1::epoc {
    dsa::dsa(window_server_client_ptr client)
        : window_client_obj(client, nullptr)
        , husband_(nullptr)
        , state_(state_none)
        , sync_thread_(nullptr)
        , sync_status_(0)
        , requester_thread_(nullptr) {
        kernel_system *kern = client->get_ws().get_kernel_system();

        // Each message is an integer. Allow maximum of 10 messages
        dsa_must_abort_queue_ = kern->create<kernel::msg_queue>("DsaAbortQueue", 4, 10);
        dsa_complete_queue_ = kern->create<kernel::msg_queue>("DsaCompleteQueue", 4, 10);

        if (client->client_version().build <= WS_V6_BUILD_VER) {
            epoc::chunk_allocator *allocator = client->get_ws().allocator();

            sync_status_ = allocator->to_address(allocator->allocate_struct<epoc::request_status>(
                epoc::status_pending, kern->is_eka1()), nullptr);

            // Leech on the owner
            sync_thread_ = kern->create<kernel::thread>(kern->get_memory_system(), kern->get_ntimer(),
                client->get_client()->owning_process(), kernel::access_type::local_access, fmt::format("DSA sync thread {}", id),
                client->get_ws().sync_thread_code_address(), 0x1000, 0, 0x1000, false, sync_status_.ptr_address());

            sync_thread_->resume();
        }
    };

    dsa::~dsa() {
        do_cancel();
        
        if (sync_thread_) {
            sync_thread_->stop();
            
            epoc::chunk_allocator *allocator = client->get_ws().allocator();
            allocator->free(allocator->to_pointer(sync_status_.ptr_address(), nullptr));

            sync_thread_ = nullptr;
        }
    }

    void dsa::request_access(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        if (state_ != state_none) {
            if (state_ != state_completed) {
                LOG_ERROR(SERVICE_WINDOW, "Requesting access on a DSA in progress");
                ctx.complete(epoc::error_argument);
                return;
            } else {
                state_ = state_none;

                if (sync_thread_) {
                    sync_thread_->resume();
                }
            }
        }

        std::uint32_t window_handle = 0;
        
        if (sync_thread_ && (cmd.header.cmd_len == 8)) {
            // First integer looks like a sync status too. Probably for other side around.
            // TODO: Use it.
            window_handle = *(reinterpret_cast<std::uint32_t *>(cmd.data_ptr) + 1);
            
            epoc::notify_info to_abort_nof;
            to_abort_nof.sts = *reinterpret_cast<address*>(cmd.data_ptr);
            to_abort_nof.requester = ctx.msg->own_thr;
            requester_thread_ = to_abort_nof.requester;

            dsa_must_abort_queue_->notify_available(to_abort_nof);
        } else {
            window_handle = *reinterpret_cast<std::uint32_t*>(cmd.data_ptr);
        }

        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(client->get_object(window_handle));

        // what the fuck msvc
        if ((!user) || (user->type != epoc::window_kind::client)) {
            LOG_ERROR(SERVICE_WINDOW, "Invalid window handle given 0x{:X}", window_handle);
            ctx.complete(epoc::error_argument);
            return;
        }

        husband_ = user;

        // We allow the whole window region to be DSAed!
        // But what is the point... To override a DSA? Should that be possible...
        // TODO: Verify...
        if (husband_->is_dsa_active()) {
            LOG_WARN(SERVICE_WINDOW, "Husband window is currently active in a DSA, silently pass");

            ctx.complete(0);
            return;
        }

        LOG_TRACE(SERVICE_WINDOW, "DSA requested for window {}", user->id);

        state_ = state_prepare;
        husband_->set_dsa_active(true);
        husband_->direct = this;

        eka2l1::rect extent;
        extent.top = husband_->pos;
        extent.size = husband_->size;

        husband_->scr->dsa_rect.merge(extent);

        ctx.complete(1);
    }

    void dsa::get_region(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        std::uint32_t max_rects = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

        if ((max_rects == 0) || (state_ != state_prepare)) {
            // What? Nothing?
            if (sync_thread_) {
                // Old DSA want 0
                ctx.complete(0);
            } else {
                ctx.complete(epoc::CINT32_MAX);
            }

            return;
        }

        // The whole window for you!
        eka2l1::rect extent;
        extent.top = husband_->pos;
        extent.size = husband_->size;

        state_ = state_running;

        ctx.write_data_to_descriptor_argument<eka2l1::rect>(reply_slot, extent);
        ctx.complete(1);
    }

    void dsa::do_cancel() {
        state_ = state_completed;

        if (husband_) {
            husband_->set_dsa_active(false);
            husband_->direct = nullptr;
            husband_ = nullptr;
        }

        if (sync_thread_) {
            sync_thread_->suspend();
        }

        dsa_must_abort_queue_->cancel_data_available(requester_thread_);
        requester_thread_ = nullptr;
    }

    void dsa::get_sync_info(service::ipc_context &ctx, ws_cmd &cmd) {
        struct sync_info {
            std::uint32_t sync_thread_handle_;
            eka2l1::ptr<epoc::request_status> sync_status_;
        } info;

        info.sync_thread_handle_ = static_cast<std::uint32_t>(sync_thread_->unique_id());
        info.sync_status_ = sync_status_;

        ctx.write_data_to_descriptor_argument<sync_info>(reply_slot, info);
        ctx.complete(epoc::error_none);
    }

    void dsa::cancel(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        do_cancel();
        ctx.complete(epoc::error_none);
    }

    bool dsa::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        ws_dsa_op op = static_cast<decltype(op)>(cmd.header.op);
        bool quit = false;

        if (client->client_version().build <= WS_V6_BUILD_VER) {
            switch (op) {
            case ws_dsa_old_get_sync_thread:
                get_sync_info(ctx, cmd);
                break;

            case ws_dsa_old_request:
                request_access(ctx, cmd);
                break;

            case ws_dsa_old_get_region:
                get_region(ctx, cmd);
                break;

            case ws_dsa_old_cancel:
                cancel(ctx, cmd);
                break;

            default:
                LOG_ERROR(SERVICE_WINDOW, "Unimplemented DSA opcode {}", cmd.header.op);
                break;
            }
        } else {
            switch (op) {
            case ws_dsa_get_send_queue:
            case ws_dsa_get_rec_queue: {
                kernel_system *kern = client->get_ws().get_kernel_system();
                std::int32_t target_handle = 0;

                target_handle = kern->open_handle_with_thread(ctx.msg->own_thr, (op == ws_dsa_get_send_queue) ? dsa_must_abort_queue_ : dsa_complete_queue_, kernel::owner_type::process);

                ctx.complete(target_handle);
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
                LOG_ERROR(SERVICE_WINDOW, "Unimplemented DSA opcode {}", cmd.header.op);
                break;
            }
            }
        }

        return quit;
    }
}
