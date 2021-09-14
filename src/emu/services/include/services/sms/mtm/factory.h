/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/msv/factory.h>
#include <services/msv/common.h>
#include <cstdint>

namespace eka2l1::epoc::sms {
    enum command_code {
        command_code_schedule_copy = 10002
    };

    class sms_operation_factory: public epoc::msv::operation_factory {
    public:
        sms_operation_factory()
            : epoc::msv::operation_factory(epoc::msv::MSV_MSG_TYPE_UID) {
        }

        std::shared_ptr<epoc::msv::operation> create_operation(const epoc::msv::msv_id operation_id, const epoc::msv::operation_buffer &buffer,
            epoc::notify_info complete_info, const std::uint32_t command) override;
    };
}