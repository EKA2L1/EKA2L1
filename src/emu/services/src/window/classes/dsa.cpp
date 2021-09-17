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
        , sync_status_(0) {
        kernel_system *kern = client->get_ws().get_kernel_system();

        // Each message is an integer. Allow maximum of 10 messages
        dsa_must_abort_queue_ = kern->create<kernel::msg_queue>("DsaAbortQueue", 4, 10);
        dsa_complete_queue_ = kern->create<kernel::msg_queue>("DsaCompleteQueue", 4, 10);

        if ((client->client_version().build <= WS_NEWARCH_VER) && (kern->get_epoc_version() <= epocver::epoc81a)) {
            epoc::chunk_allocator *allocator = client->get_ws().allocator();

            sync_status_ = allocator->to_address(allocator->allocate_struct<epoc::request_status>(
                                                     epoc::status_pending, kern->is_eka1()),
                nullptr);

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

            dsa_must_stop_notify_.sts = *reinterpret_cast<address *>(cmd.data_ptr);
            dsa_must_stop_notify_.requester = ctx.msg->own_thr;
        } else {
            window_handle = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);
        }

        epoc::window_user *user = reinterpret_cast<epoc::window_user *>(client->get_object(window_handle));

        // what the fuck msvc
        if ((!user) || (user->type != epoc::window_kind::client)) {
            LOG_ERROR(SERVICE_WINDOW, "Invalid window handle given 0x{:X}", window_handle);
            ctx.complete(epoc::error_argument);
            return;
        }

        husband_ = user;
        state_ = state_prepare;

        if (husband_->is_dsa_active()) {
            LOG_WARN(SERVICE_WINDOW, "Husband window is currently active in a DSA, adding reference");
        }

        LOG_TRACE(SERVICE_WINDOW, "DSA requested for window {}", user->id);

        husband_->add_dsa_active(this);
        husband_->scr->ref_dsa_usage();

        operate_region_ = husband_->visible_region;

        ctx.complete(static_cast<int>(operate_region_.rects_.size()));
    }

    void dsa::get_region(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        std::uint32_t max_rects = *reinterpret_cast<std::uint32_t *>(cmd.data_ptr);

        if (state_ == state_prepare) {
            if (operate_region_.rects_.size() != max_rects) {
                ctx.complete(static_cast<int>(operate_region_.rects_.size()));
                return;
            } else {
                eka2l1::rect *data_ptr = reinterpret_cast<eka2l1::rect*>(ctx.get_descriptor_argument_ptr(reply_slot));
                const std::size_t data_bsize = ctx.get_argument_max_data_size(reply_slot);

                if (data_bsize != max_rects * sizeof(eka2l1::rect)) {
                    LOG_ERROR(SERVICE_WINDOW, "The length of region rects buffer is contradicting! (expected {}, got {})", max_rects * sizeof(eka2l1::rect), data_bsize);
                    ctx.complete(epoc::error_argument);

                    return;
                }

                if (!data_ptr) {
                    ctx.complete(epoc::error_argument);
                    return;
                }

                for (std::size_t i = 0; i < operate_region_.rects_.size(); i++) {
                    data_ptr[i] = operate_region_.rects_[i];
                    data_ptr[i].transform_to_symbian_rectangle();
                }

                ctx.set_descriptor_argument_length(reply_slot, max_rects * sizeof(eka2l1::rect));
            }
        }

        state_ = state_running;

        if ((sync_thread_) && (client->client_version().build <= WS_OLDARCH_VER)) {
            // Old DSA want 0
            ctx.complete(0);
        } else {
            ctx.complete(epoc::CINT32_MAX);
        }
    }

    void dsa::visible_region_changed(const common::region &new_region) {
        // There might be race condition when we notify region changed (due to notification)
        // For safety, just do a copy and assign first
        common::region old_one = operate_region_;
        operate_region_ = new_region;

        if ((state_ == state_running) && !old_one.identical(new_region)) {
            abort(dsa_terminate_region);
        }
    }

    void dsa::do_cancel() {
        state_ = state_completed;

        if (husband_) {
            husband_->scr->deref_dsa_usage();

            husband_->remove_dsa_active(this);
            husband_ = nullptr;
        }

        if (sync_thread_) {
            sync_thread_->suspend();
        }

        dsa_must_stop_notify_.complete(epoc::error_cancel);
    }

    void dsa::abort(const std::int32_t reason) {
        if (!dsa_must_abort_queue_->send(&reason, sizeof(reason))) {
            LOG_ERROR(SERVICE_WINDOW, "Unable to send abort message code {} to client", reason);
        }

        do_cancel();
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

    void dsa::free(eka2l1::service::ipc_context &context, eka2l1::ws_cmd &cmd) {
        on_command_batch_done(context);

        context.complete(epoc::error_none);
        client->delete_object(cmd.obj_handle);
    }

    bool dsa::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        ws_dsa_op op = static_cast<decltype(op)>(cmd.header.op);
        bool quit = false;

        kernel_system *kern = client->get_ws().get_kernel_system();

        if (client->client_version().build <= WS_OLDARCH_VER || kern->get_epoc_version() <= epocver::epoc80) {
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
            case ws_dsa_get_sync_thread:
                get_sync_info(ctx, cmd);
                break;

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

            case ws_dsa_free:
                free(ctx, cmd);
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
