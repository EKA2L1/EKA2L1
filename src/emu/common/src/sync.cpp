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

#include <common/sync.h>

namespace eka2l1::common {
    semaphore::semaphore(const int initial)
        : count_(initial) {
    }

    void semaphore::notify(const int count) {
        std::unique_lock<std::mutex> ulock(mut_);
        int to_dec = count;

        while (to_dec-- > 0) {
            count_++;

            if (count_ <= 0) {
                cond_.notify_one();
            }
        }
    }

    bool semaphore::wait(const std::size_t duration_us) {
        std::unique_lock<std::mutex> ulock(mut_);
        count_ -= 1;

        if (count_ < 0) {
            if (duration_us > 0) {
                if (cond_.wait_for(ulock, std::chrono::microseconds(duration_us)) == std::cv_status::timeout) {
                    // Timeout, refund the wait
                    count_++;
                    return true;
                }

                return false;
            }
            
            cond_.wait(ulock);
        }

        return false;
    }

    event::event():
        is_set_(false) {
    }

    void event::set() {
        const std::lock_guard<std::mutex> guard(lock_);

        if (!is_set_) {
            is_set_ = true;
            cond_.notify_one();
        }
    }

    void event::wait() {
        std::unique_lock<std::mutex> unq(lock_);
        cond_.wait(unq, [this] { return is_set_; });

        is_set_ = false;
    }

    bool event::wait_for(const std::uint64_t duration_us) {
        std::unique_lock<std::mutex> unq(lock_);
        
        if (!cond_.wait_for(unq, std::chrono::microseconds(duration_us), [this] { return is_set_; }))
            return false;

        is_set_ = false;
        return true;
    }

    void event::reset() {
        const std::lock_guard<std::mutex> guard(lock_);
        is_set_ = false;
    }
}