/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/msv/operations/base.h>

namespace eka2l1::epoc::sms {
    class schedule_copy_operation: public epoc::msv::operation {
    private:
        epoc::msv::system_progress_info sys_progress_;

    public:
        explicit schedule_copy_operation(const epoc::msv::msv_id operation_id, const epoc::msv::operation_buffer &buffer,
            epoc::notify_info complete_info);

        void execute(msv_server *server, const kernel::uid process_uid) override;
        std::int32_t system_progress(epoc::msv::system_progress_info &progress) override;
    };
}