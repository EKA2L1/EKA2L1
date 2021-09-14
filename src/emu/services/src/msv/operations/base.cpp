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

#include <services/msv/operations/base.h>
#include <utils/err.h>

namespace eka2l1::epoc::msv {
    operation::operation(const msv_id operation_id, const operation_buffer &buffer, epoc::notify_info complete_info)
        : operation_id_(operation_id)
        , state_(operation_state_idle)
        , buffer_(buffer)
        , complete_info_(std::move(complete_info)) {

    }

    void operation::cancel() {
        complete_info_.complete(epoc::error_cancel);
    }

    std::int32_t operation::system_progress(system_progress_info &progress) {
        return epoc::error_not_supported;
    }
}