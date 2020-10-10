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

#pragma once

#include <cstdint>

namespace eka2l1::common {
    /**
     * \brief Rename caller thread.
     * 
     * \param thread_name       New name of the thread.
     */
    void set_thread_name(const char *thread_name);

    enum thread_priority {
        thread_priority_low,
        thread_priority_normal,
        thread_priority_high,
        thread_priority_very_high
    };

    /**
     * @brief Set current thread priority
     * 
     * @param pri New priority of the thread.
     * @see   set_thread_name
     */
    void set_thread_priority(const thread_priority pri);
}