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
#include <epoc/kernel.h>
#include <epoc/kernel/kernel_obj.h>

#include <common/chunkyseri.h>

#include <cstdio>

namespace eka2l1 {
    namespace kernel {
        kernel_obj::kernel_obj(kernel_system *kern, const std::string &obj_name, kernel::access_type access)
            : obj_name(obj_name)
            , kern(kern)
            , access(access)
            , uid(kern->next_uid()) {
        }

        void kernel_obj::do_state(common::chunkyseri &seri) {
            auto s = seri.section("KernelObject", 1);

            if (!s) {
                return;
            }

            seri.absorb(obj_name);
            seri.absorb(uid);
            seri.absorb(obj_type);
            seri.absorb(access);
            seri.absorb(access_count);
        }
    }
}
