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

#include <core/ipc.h>

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
}