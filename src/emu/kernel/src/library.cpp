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

#include <common/cvt.h>

#include <kernel/codeseg.h>
#include <kernel/library.h>

#include <kernel/libmanager.h>

#include <kernel/kernel.h>
#include <mem/mem.h>

#include <common/chunkyseri.h>
#include <common/log.h>

namespace eka2l1 {
    namespace kernel {
        library::library(kernel_system *kern, codeseg_ptr codeseg)
            : kernel_obj(kern, codeseg->name(), nullptr, access_type::global_access)
            , codeseg(std::move(codeseg))
            , reffed(false) {
            obj_type = object_type::library;
        }

        std::optional<uint32_t> library::get_ordinal_address(kernel::process *pr, const std::uint32_t idx) {
            return codeseg->lookup(pr, idx);
        }

        std::vector<uint32_t> library::attach(kernel::process *pr) {
            if (codeseg->state_with(pr) == codeseg_state_detached) {
                std::vector<std::uint32_t> call_list;

                codeseg->attach(pr);
                codeseg->queries_call_list(pr, call_list);

                codeseg->unmark();

                reffed = true;

                return call_list;
            }

            return std::vector<uint32_t>{};
        }

        void library::destroy() {
            if (reffed) {
                codeseg->deref(kern->crr_thread());
            }
        }

        bool library::attached(kernel::process *pr) {
            return codeseg->attached_report(pr);
        }

        void library::do_state(common::chunkyseri &seri) {
            // TODO
        }
    }
}