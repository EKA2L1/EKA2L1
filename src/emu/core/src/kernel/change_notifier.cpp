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

#include <core/core_kernel.h>
#include <core/core_mem.h>
#include <core/ptr.h>

#include <core/kernel/change_notifier.h>

#include <common/cvt.h>
#include <common/random.h>

namespace eka2l1 {
    namespace kernel {
        change_notifier::change_notifier(kernel_system *kern)
            : eka2l1::kernel::kernel_obj(kern, "changenotifier" + common::to_string(eka2l1::random()),
                  kernel::access_type::local_access) {
            obj_type = object_type::change_notifier;
        }

        bool change_notifier::logon(eka2l1::ptr<epoc::request_status> request_sts) {
            if (request_status) {
                return false;
            }

            requester = kern->crr_thread();
            request_status = request_sts;

            return true;
        }

        bool change_notifier::logon_cancel() {
            if (!request_status) {
                return false;
            }

            *request_status.get(kern->get_memory_system()) = -3;
            requester->signal_request();

            requester = nullptr;
            request_status = 0;

            return true;
        }

        void change_notifier::notify_change_requester() {
            *request_status.get(kern->get_memory_system()) = 0;
            requester->signal_request();

            requester = nullptr;
            request_status = 0;
        }

        void change_notifier::write_object_to_snapshot(common::wo_buf_stream &stream) {
            kernel_obj::write_object_to_snapshot(stream);

            const std::uint64_t requester_id = requester->unique_id();

            stream.write(&requester_id, sizeof(requester_id));
            stream.write(&request_status, sizeof(request_status));
        }

        void change_notifier::do_state(common::ro_buf_stream &stream) {
            std::uint64_t requester_id = 0;
            stream.read(&requester_id, sizeof(requester_id));
            stream.read(&request_status, sizeof(request_status));

            requester = std::dynamic_pointer_cast<kernel::thread>
                (kern->get_kernel_obj_by_id(requester_id));
        }
    }
}