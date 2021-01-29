/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <kernel/kernel.h>
#include <mem/ptr.h>

#include <kernel/undertaker.h>

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/random.h>

#include <utils/err.h>

namespace eka2l1::kernel {
    undertaker::undertaker(kernel_system *kern)
        : eka2l1::kernel::kernel_obj(kern, "Undertaker" + common::to_string(eka2l1::random()),
            nullptr, kernel::access_type::local_access) {
        obj_type = object_type::undertaker;
    }

    bool undertaker::logon(kernel::thread *requester, eka2l1::ptr<epoc::request_status> request_sts,
        eka2l1::ptr<kernel::handle> thr_handle) {
        // If this is already logon
        if (!req_info_.empty()) {
            return false;
        }

        req_info_ = epoc::notify_info { request_sts, requester };
        thread_handle_ = thr_handle;

        return true;
    }

    bool undertaker::logon_cancel() {
        if (req_info_.empty()) {
            return false;
        }

        req_info_.complete(epoc::error_cancel);
        return true;
    }

    void undertaker::complete(kernel::thread *req) {
        if (!req_info_.empty()) {
            kernel::handle *hptr = thread_handle_.get(req->owning_process());
            if (hptr) {
                kernel::handle hopened = kern->open_handle_with_thread(req_info_.requester, req, kernel::owner_type::thread);
                if (hopened == INVALID_HANDLE) {
                    LOG_ERROR(KERNEL, "Unable to clone new handle to notify undertaker!");
                } else {
                    *hptr = hopened;
                }
            } else {
                LOG_WARN(KERNEL, "Thread handle pointer is invalid (0x{:X})", thread_handle_.ptr_address());
            }

            // Complete with thread die status code
            req_info_.complete(epoc::error_died);
        }
    }

    void undertaker::do_state(common::chunkyseri &seri) {
        req_info_.do_state(seri);
        thread_handle_.do_state(seri);
    }
}