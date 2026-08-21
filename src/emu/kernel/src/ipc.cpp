/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/log.h>

#include <kernel/ipc.h>
#include <kernel/session.h>
#include <kernel/thread.h>

namespace eka2l1 {
    ipc_arg::ipc_arg(int arg0, const int aflag) {
        args[0] = arg0;
        flag = aflag;
    }

    ipc_arg::ipc_arg(int arg0, int arg1, const int aflag) {
        args[0] = arg0;
        args[1] = arg1;
        flag = aflag;
    }

    ipc_arg::ipc_arg(int arg0, int arg1, int arg2, const int aflag) {
        args[0] = arg0;
        args[1] = arg1;
        args[2] = arg2;

        flag = aflag;
    }

    ipc_arg::ipc_arg(int arg0, int arg1, int arg2, int arg3, const int aflag) {
        args[0] = arg0;
        args[1] = arg1;
        args[2] = arg2;
        args[3] = arg3;

        flag = aflag;
    }

    ipc_arg_type ipc_arg::get_arg_type(int slot) {
        return static_cast<ipc_arg_type>((flag >> (slot * 3)) & 7);
    }

    ipc_msg::ipc_msg(kernel::thread *own)
        : own_thr(own)
        , function(0)
        , msg_session(nullptr)
        , session_ptr_lle(0)
        , request_sts(0)
        , msg_status(ipc_message_status::none)
        , id(0)
        , thread_handle_low(0)
        , ref_count(0)
        , type(ipc_message_type_wild) {
    }

    ipc_msg::~ipc_msg() {
        if (ref_count != 0) {
            ref_count = 1;
            unref();
        }
    }

    void ipc_msg::ref() {
        if (ref_count == 0) {
            if (own_thr) {
                own_thr->increase_access_count();
            }
        }

        ref_count++;
    }

    void ipc_msg::unref() {
        // Guard against a double unref (e.g. a server completing a message
        // whose client already released it). Wrapping the unsigned count
        // would poison the slot forever and desync the owner thread's
        // access accounting.
        if (ref_count == 0) {
            LOG_WARN(KERNEL, "Unreferencing an already-released IPC message (id {}, function 0x{:X})", id, function);
            return;
        }

        if (--ref_count == 0) {
            if (msg_session) {
                session_msg_link.deque();
            }

            // A message released while still sitting in a server's delivered
            // queue (its session died before the server received it) must
            // leave that queue too, or the server later pops the freed slot -
            // possibly recycled by a new exchange by then - and completes it
            // again against the wrong owner.
            if (delivered_msg_link.next) {
                delivered_msg_link.deque();
            }

            // Read this before dropping the access count: the drop may destroy
            // a stopped owner on the spot, and own_thr must not be touched after.
            const bool owner_stopped = own_thr && (own_thr->current_state() == kernel::thread_state::stop);

            if (own_thr) {
                own_thr->decrease_access_count();
            }

            switch (type) {
            case ipc_message_type_disconnect:
                type = ipc_message_type_wild;
                break;

            case ipc_message_type_sync:
                // A live thread keeps its sync message for reuse, so the slot
                // normally stays reserved. Only reclaim it when the owner died
                // with this message still in flight (free_msg skipped it then).
                // The drop above may have destroyed that owner, so drop the
                // pointer along with the slot reservation.
                if (owner_stopped) {
                    type = ipc_message_type_wild;
                    own_thr = nullptr;
                }

                break;

            case ipc_message_type_session:
                if (msg_session) {
                    msg_session->set_slot_free(this);
                } else {
                    type = ipc_message_type_wild;
                }

                break;
            }

            msg_session = nullptr;
        }
    }
}