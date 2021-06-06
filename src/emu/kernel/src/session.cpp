/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <kernel/server.h>
#include <kernel/session.h>

#include <kernel/kernel.h>

#include <common/log.h>
#include <utils/err.h>

namespace eka2l1 {
    namespace service {
        session::session(kernel_system *kern, server_ptr svr, int async_slot_count)
            : kernel_obj(kern, "", nullptr, kernel::access_type::global_access) 
            , svr(svr)
            , cookie_address(0)
            , headless_(false) {
            obj_type = kernel::object_type::session;

            svr->attach(this);

            if (async_slot_count > 0) {
                msgs_pool.resize(async_slot_count);

                for (auto &msg : msgs_pool) {
                    msg = std::make_pair(true, kern->create_msg(kernel::owner_type::process));
                    msg.second->type = ipc_message_type_session;
                }
            }

            disconnect_msg_ = kern->create_msg(kernel::owner_type::process);

            disconnect_msg_->type = ipc_message_type_disconnect;
            disconnect_msg_->function = standard_ipc_message_disconnect;
            disconnect_msg_->own_thr = kern->crr_thread();
            disconnect_msg_->request_sts = 0;
        }

        // Disconnect
        session::~session() {
        }

        void session::set_share_mode(const share_mode shmode) {
            if (owner) {
                owner->decrease_access_count();
            }

            switch (shmode) {
            case SHARE_MODE_UNSHAREABLE:
                owner = kern->crr_thread();
                break;

            case SHARE_MODE_SHAREABLE:
                owner = kern->crr_process();
                break;

            case SHARE_MODE_GLOBAL_SHAREABLE:
                owner = nullptr;
                break;

            default:
                break;
            }

            shmode_ = shmode;

            if (owner) {
                owner->increase_access_count();
            }
        }

        ipc_msg_ptr session::get_free_msg() {
            if (msgs_pool.empty()) {
                return kern->create_msg(kernel::owner_type::process);
            }

            auto free_msg_in_pool = std::find_if(msgs_pool.begin(), msgs_pool.end(),
                [](const auto &msg) { return msg.first; });

            if (free_msg_in_pool != msgs_pool.end()) {
                free_msg_in_pool->first = false;
                return free_msg_in_pool->second;
            }

            return ipc_msg_ptr(nullptr);
        }

        void session::set_slot_free(ipc_msg *msg) {
            auto wrap_msg = std::find_if(msgs_pool.begin(), msgs_pool.end(),
                [&](const auto &wrap_msg) { return wrap_msg.second == msg; });

            if (wrap_msg != msgs_pool.end()) {
                wrap_msg->first = true;
            }
        }

        bool session::eligible_to_send(kernel::thread *thr) {
            if (!owner) {
                return true;
            }

            if (owner->get_object_type() == kernel::object_type::thread) {
                if (owner != thr) {
                    return false;
                }
            } else {
                if (owner != thr->owning_process()) {
                    return false;
                }
            }

            return true;
        }

        // This behaves a little different then other
        int session::send_receive_sync(const int function, const ipc_arg &args, eka2l1::ptr<epoc::request_status> request_sts) {
            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->own_thr = kern->crr_thread();
            msg->request_sts = request_sts;

            return send(msg);
        }

        int session::send_receive(const int function, const ipc_arg &args, eka2l1::ptr<epoc::request_status> request_sts) {
            ipc_msg_ptr msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->request_sts = request_sts;
            msg->own_thr = kern->crr_thread();

            return send(msg);
        }

        int session::send(ipc_msg_ptr msg) {
            msg->ref();

            if (!eligible_to_send(kern->crr_thread())) {
                msg->unref();
                return epoc::error_permission_denied;
            }

            msg->msg_session = (headless_) ? nullptr : this;
            msg->session_ptr_lle = cookie_address;

            in_progress_msgs_.push(&msg->session_msg_link);

            return svr->deliver(msg);
        }

        void session::send_destruct() {
            headless_ = !svr->is_hle();
            send(disconnect_msg_);
        }

        void session::detatch(const int error_code) {
            if (!svr) {
                return;
            }

            // Complete all messages in progress
            while (!in_progress_msgs_.empty()) {
                ipc_msg *msg = E_LOFF(in_progress_msgs_.first()->deque(), ipc_msg, session_msg_link);

                if (msg) {
                    if ((msg->msg_status == ipc_message_status::accepted) || (msg->msg_status == ipc_message_status::delivered)) {
                        if (msg->own_thr && (msg->own_thr->current_state() != kernel::thread_state::stop)) {
                            epoc::request_status *final_sts = msg->request_sts.get(msg->own_thr->owning_process());
                            if (final_sts) {
                                msg->msg_status = ipc_message_status::completed;

                                final_sts->set(error_code, kern->is_eka1());
                                msg->own_thr->signal_request();
                            }
                        }

                        msg->unref();
                    }
                }
            }
        }

        void session::destroy() {
            detatch(epoc::error_session_closed);

            if (kern->crr_thread()) {
                // Try to send a disconnect message. Headless session and use sync message.
                if (svr) {
                    send_destruct();

                    if (svr->is_hle()) {
                        svr->process_accepted_msg();
                    }
                }
            }

            // Free the message pool anyway
            for (const auto &msg : msgs_pool) {
                kern->free_msg(msg.second);
            }
            
            if (svr) {
                svr->detach(this);

                if (!svr->is_hle()) {
                    svr = nullptr;
                }
            }

            if (owner)
                owner->decrease_access_count();
        }
    }
}
