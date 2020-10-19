/*
 * Copyright (c) 2019 EKA2L1 Team / 2020 yuzu Emulator Project.
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

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace eka2l1::common {
    class semaphore {
        std::mutex mut_;
        std::condition_variable cond_;

        std::atomic<int> count_;

    public:
        explicit semaphore(const int inital = 0);

        /**
         * @brief       Notify the semaphore.
         * 
         * The semaphore count will be increased by the number provided.
         * 
         * The count is consecutively increased by 1, and if the internal semaphore count is still
         * smaller or equal than 0, a thread being held by this semaphore is released.
         * 
         * @param       count       The number to increase semaphore count.
         */
        void notify(const int count = 1);

        /**
         * @brief       Wait for the semaphore indefinitely or until a certain amount of time.
         * 
         * If the semaphore count is bigger or equal to 0, this is returned immediately. Else
         * the wait happens.
         * 
         * In the case of waiting for a certain amount of time (in microseconds unit), the count
         * is refunded if timeout is hit. In that case, this function returns true, indicating
         * that timeout is reached.
         * 
         * @param       duration_us The amount of time to wait, in microseconds. Use 0 to wait indefinitely.
         * @returns     True if timeout is hit.
         */
        bool wait(const std::size_t duration_us = 0);
    };

    class event_impl;

    class event {
    private:
        event_impl *impl_;

    public:
        explicit event();
        ~event();

        void set();

        void wait();
        bool wait_for(const std::uint64_t duration_us);

        void reset();
    };
}