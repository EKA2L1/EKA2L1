/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <epoc/kernel/process.h>
#include <epoc/kernel/thread.h>
#include <epoc/utils/reqsts.h>

#include <common/chunkyseri.h>

namespace eka2l1::epoc {
    void request_status::do_state(common::chunkyseri &seri) {
        seri.absorb(status);
        seri.absorb(flags);
    }

    void notify_info::complete(int err_code) {
        if (sts.ptr_address() == 0) {
            return;
        }

        *sts.get(requester->owning_process()) = err_code;
        sts = 0;

        requester->signal_request();
    }

    void notify_info::do_state(common::chunkyseri &seri) {
        sts.do_state(seri);

        auto thread_uid = requester->unique_id();
        seri.absorb(thread_uid);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            // TODO: Get the thread
        }
    }
}
