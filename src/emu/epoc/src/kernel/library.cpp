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

#include <epoc/kernel/library.h>
#include <epoc/kernel/codeseg.h>

#include <epoc/kernel/libmanager.h>

#include <epoc/kernel.h>
#include <epoc/mem.h>

#include <common/chunkyseri.h>
#include <common/log.h>

namespace eka2l1 {
    namespace kernel {
        library::library(kernel_system *kern, codeseg_ptr codeseg)
            : kernel_obj(kern, codeseg->name(), access_type::global_access)
            , codeseg(std::move(codeseg)) {
            obj_type = object_type::library;
            state = library_state::loaded;
        }

        std::optional<uint32_t> library::get_ordinal_address(const uint8_t idx) {
            return codeseg->lookup(idx);
        }

        std::vector<uint32_t> library::attach() {
            if (state == library_state::loaded) {
                state = library_state::attaching;
                
                std::vector<std::uint32_t> call_list;
                codeseg->queries_call_list(call_list);

                return call_list;
            }

            return std::vector<uint32_t>{};
        }

        bool library::attached() {
            if (state == library_state::attached || state != library_state::attaching) {
                return false;
            }

            state = library_state::attached;
            return true;
        }

        void library::do_state(common::chunkyseri &seri) {
            seri.absorb(state);
            // TODO
        }
    }
}