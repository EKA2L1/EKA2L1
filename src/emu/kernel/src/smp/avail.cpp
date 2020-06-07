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

#include <kernel/smp/avail.h>

namespace eka2l1::kernel::smp {
    cpu_availability::cpu_availability(const std::uint32_t num_cores)
        : remains(num_cores, idle_unit) {
        total_remain = idle_unit * num_cores;
    }

    const std::size_t cpu_availability::num_cores() const {
        return remains.size();
    }

    bool cpu_availability::add_load(const std::uint32_t cpu_index, const std::uint32_t load_unit) {
        if (cpu_index >= static_cast<std::uint32_t>(remains.size())) {
            return false;
        }

        std::int32_t original = remains[cpu_index];
        remains[cpu_index] -= load_unit;

        if (remains[cpu_index] < maxed_out_unit) {
            remains[cpu_index] = maxed_out_unit;
        }

        if (original > 0) {
            // It shouldn't be negative
            total_remain -= (static_cast<std::uint32_t>(original) > load_unit) ? load_unit : original;
        }

        return true;
    }

    std::uint32_t cpu_availability::find_lowest_load() const {
        std::size_t index = 0;
        std::int32_t maximum_load = -1;

        for (std::size_t i = 0; i < remains.size(); i++) {
            if (maximum_load < remains[i]) {
                index = i;
                maximum_load = remains[i];
            }
        }

        return static_cast<std::uint32_t>(index);
    }

    bool cpu_availability::set_load_max(const std::uint32_t cpu_index) {
        if (cpu_index >= static_cast<std::uint32_t>(remains.size())) {
            return false;
        }

        if (remains[cpu_index] > 0) {
            // Since we maxed out the load of this core, we need to also add the load to total remains.
            total_remain -= remains[cpu_index];
        }

        remains[cpu_index] = maxed_out_unit;
        return true;
    }
}