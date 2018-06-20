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

#include <ptr.h>
#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;

    enum class ipc_arg_type {
        unspecified = 0,
        handle = 1,
        flag_des = 4,
        flag_const = 2,
        flag_16b = 1,
        des8 = flag_des,
        des16 = flag_des | des8,
        desc8 = des8 | flag_const,
        desc16 = des16 | flag_const
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

    class session;
    using session_ptr = std::shared_ptr<session>;

    enum class ipc_message_status {
        delivered,
        accepted,
        completed
    };

    /* An IPC msg (ver 2) contains the IPC context. */
    /* Function: The IPC function ordinal */
    /* Arg: IPC args. Max args = 4 */
    /* Session: Pointer to the session. */
    struct ipc_msg {
        thread_ptr own_thr;
        int function;
        ipc_arg args;
        session_ptr msg_session;

        int *request_sts;

        // Status of the message, if it's accepted or delivered
        ipc_message_status msg_status;
        uint64_t id;

        int owner_type;
        uint32_t owner_id;

        bool free : true;

        ipc_msg() {}

        ipc_msg(uint64_t id, uint32_t owner_id, thread_ptr thr)
            : id(id), own_thr(thr), owner_id(owner_id) {}
    };

    using ipc_msg_ptr = std::shared_ptr<ipc_msg>;
}