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

#include <services/msv/common.h>
#include <utils/reqsts.h>

#include <cstdint>

namespace eka2l1 {
    class msv_server;

    namespace kernel {
        using uid = std::uint64_t;
    }
}

namespace eka2l1::epoc::msv {
    enum operation_state {
        operation_state_idle,
        operation_state_queued,         // The operation has been queued for asynchronous execution.
        operation_state_pending,        // The operation is in progress.
        operation_state_success,
        operation_state_failure
    };

    /**
     * @brief Abstraction class represents a MSV task that can be asynchronous.
     */
    class operation {
    private:
        msv_id operation_id_;
        operation_buffer progress_;
        operation_state state_;

    protected:
        operation_buffer buffer_;
        epoc::notify_info complete_info_;

        template <typename T>
        T *progress_data() {
            if (progress_.size() < sizeof(T)) {
                progress_.resize(sizeof(T));
            }

            return reinterpret_cast<T*>(progress_.data());
        }

    public:
        explicit operation(const msv_id operation_id, const operation_buffer &buffer,
            epoc::notify_info complete_info);

        virtual void execute(msv_server *server, const kernel::uid process_uid) = 0;
        virtual void cancel();

        const msv_id operation_id() const {
            return operation_id_;
        }
        
        const operation_buffer &progress_buffer() const {
            return progress_;
        }

        const operation_state state() const {
            return state_;
        }

        void state(const operation_state state) {
            state_ = state;
        }
    };
}