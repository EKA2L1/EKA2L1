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

#pragma once

#include <kernel/common.h>

#include <mem/ptr.h>
#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    namespace epoc {
        struct request_status;
    }

    enum class ipc_arg_type {
        unspecified = 0,
        handle = 1,
        flag_des = 4,
        flag_const = 2,
        flag_16b = 1,
        des8 = flag_des,
        des16 = flag_des | flag_16b,
        desc8 = des8 | flag_const,
        desc16 = des16 | flag_const
    };

    enum standard_ipc_message {
        standard_ipc_message_connect = -1,
        standard_ipc_message_disconnect = -2
    };

    enum {
        bits_per_type = 3
    };

    enum class ipc_arg_pin {
        pin_arg_shift = bits_per_type * 4,
        pin_arg0 = 1 << (pin_arg_shift + 0),
        pin_arg1 = 1 << (pin_arg_shift + 1),
        pin_arg2 = 1 << (pin_arg_shift + 2),
        pin_arg3 = 1 << (pin_arg_shift + 3),
        pin_mask = 0xF << pin_arg_shift
    };

    struct ipc_arg {
        int args[4];
        int flag;

        ipc_arg() {}

        ipc_arg(int arg0, const int flag);
        ipc_arg(int arg0, int arg1, const int flag);
        ipc_arg(int arg0, int arg1, int arg2, const int flag);
        ipc_arg(int arg0, int arg1, int arg2, int arg3, const int flag);

        ipc_arg_type get_arg_type(int slot);
    };

    namespace service {
        class session;
    }

    enum class ipc_message_status {
        none,
        delivered,
        accepted,
        completed
    };

    /* An IPC msg (ver 2) contains the IPC context. */
    /* Function: The IPC function ordinal */
    /* Arg: IPC args. Max args = 4 */
    /* Session: Pointer to the session. */
    struct ipc_msg {
        kernel::thread *own_thr;
        int function;
        ipc_arg args;
        service::session *msg_session;
        int session_ptr_lle; // This should be null because the server check for it

        eka2l1::ptr<epoc::request_status> request_sts;

        // Status of the message, if it's accepted or delivered
        ipc_message_status msg_status;
        std::uint32_t id;
        std::uint32_t attrib = 0;
        kernel::handle thread_handle_low = 0;

        enum {
            MSG_ATTRIB_LOCK_FREE = 0x1
        };

        bool free : true;

        void lock_free() {
            attrib |= MSG_ATTRIB_LOCK_FREE;
        }

        void unlock_free() {
            attrib &= ~MSG_ATTRIB_LOCK_FREE;
        }

        bool locked() {
            return attrib & MSG_ATTRIB_LOCK_FREE;
        }

        explicit ipc_msg(kernel::thread *own);
    };

    using ipc_msg_ptr = std::shared_ptr<ipc_msg>;
}
