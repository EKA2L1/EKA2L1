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

#pragma once

#include <kernel/kernel_obj.h>
#include <utils/reqsts.h>
#include <mem/ptr.h>

#include <memory>

namespace eka2l1::kernel {
    class thread;

    // it's a cool gun in terraria. too bad early game weapon
    class undertaker : public eka2l1::kernel::kernel_obj {
        epoc::notify_info req_info_;
        eka2l1::ptr<kernel::handle> thread_handle_;

    public:
        explicit undertaker(kernel_system *kern);

        bool logon(kernel::thread *requester, eka2l1::ptr<epoc::request_status> request_sts,
            eka2l1::ptr<kernel::handle> thr_handle);

        bool logon_cancel();

        void complete(kernel::thread *req);
        void do_state(common::chunkyseri &seri) override;
    };
}
