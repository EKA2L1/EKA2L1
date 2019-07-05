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
#include <vector>

namespace eka2l1::kernel::smp {
    /**
     * \brief Manage CPU usage and load.
     * 
     * This struct manages usage and load of every cores in the system. The load store here is a critical
     * component that let to a efficient core picking.
     */
    struct cpu_availability {
        enum : int32_t {
            idle_unit = 4095,
            maxed_out_unit = -268435456 
        };
        
        std::vector<std::int32_t> remains;          ///< The total load unit that each core (element) can handle left.
        std::int32_t total_remain;                  ///< The total load unit that all cores can handle left.

        explicit cpu_availability(const std::uint32_t num_cores);

        /**
         * \brief   Add more load unit to the specified core.
         * 
         * \param   cpu_index The index of the core.
         * \param   load_unit The total load unit to add.
         * 
         * \returns True on success, false on failure (index out of range).
         * \sa      pick_cpu
         */
        bool add_load(const std::uint32_t cpu_index, const std::uint32_t load_unit);

        /**
         * \brief Pick a core that is most availability (least loaded).
         * 
         * \param    available_cpu_mask The mask that marks core allow to pick.
         * \returns  The index of the core on success. -1 if picking failed.
         * 
         * \sa       add_laod
         */
        // std::int32_t pick_cpu(const std::uint32_t available_cpu_mask);

        /**
         * \brief   Set a core load to maximum.
         * \param   cpu_index The index of the core.
         * 
         * \returns True on success.
         */
        bool set_load_max(const std::uint32_t cpu_index);

        /**
         * \brief    Find a core that has the lowest load.
         * \returns  The index of the core.
         * 
         * \sa       set_load_max
         */
        std::uint32_t find_lowest_load() const;

        /**
         * \brief    Get number of cores.
         * \returns  Numer of cores.
         */
        const std::size_t num_cores() const;
    };
}