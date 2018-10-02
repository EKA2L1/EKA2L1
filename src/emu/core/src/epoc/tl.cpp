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

#include <core/epoc/tl.h>
#include <core/core.h>

namespace eka2l1::epoc {
    eka2l1::kernel::thread_local_data &current_local_data(eka2l1::system *sys) {
        return sys->get_kernel_system()->crr_thread()->get_local_data();
    }

    eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr) {
        return sys->get_kernel_system()->crr_thread()->get_tls_slot(addr, addr);
    }
}