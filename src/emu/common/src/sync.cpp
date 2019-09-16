/*
 * Copyright (c) 2019 EKA2L1 Team
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
        const std::unique_lock<std::mutex> ulock(mut_);
        count_ += count;

        int to_start = count;

        while (to_start-- > 0) {
            cond_.notify_one();
        }
    }

    void semaphore::wait() {
        std::unique_lock<std::mutex> ulock(mut_);
        count_ -= 1;

        if (count_ < 0) {
            cond_.wait(ulock);
        }
    }
}