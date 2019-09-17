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
#include <epoc/mem.h>
#include <epoc/ptr.h>

#include <epoc/kernel/change_notifier.h>

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/random.h>

namespace eka2l1 {
    namespace kernel {
        change_notifier::change_notifier(kernel_system *kern)
            : eka2l1::kernel::kernel_obj(kern, "changenotifier" + common::to_string(eka2l1::random()),
                nullptr, kernel::access_type::local_access) {
            obj_type = object_type::change_notifier;
        }

        bool change_notifier::logon(eka2l1::ptr<epoc::request_status> request_sts) {
            if (!req_info_.sts) {
                return false;
            }

            req_info_.requester = kern->crr_thread();
            req_info_.sts = request_sts;

            return true;
        }

        bool change_notifier::logon_cancel() {
            if (!req_info_.sts) {
                return false;
            }

            req_info_.complete(-3);

            return true;
        }

        void change_notifier::notify_change_requester() {
            req_info_.complete(0);
        }

        void change_notifier::do_state(common::chunkyseri &seri) {
            /*
            std::uint32_t requester_id = (requester ? 0 : requester->unique_id());

            seri.absorb(requester_id);
            seri.absorb(request_status.ptr_address());

            if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                requester = std::reinterpret_pointer_cast<kernel::thread>(kern->get_kernel_obj_by_id(requester_id));
            }
            */
        }
    }
}
