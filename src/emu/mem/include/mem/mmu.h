/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <mem/page.h>
#include <memory>

namespace eka2l1::arm {
    class core;
}

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::mem {
    class control_base;

    /**
     * \brief The base of memory management unit.
     */
    class mmu_base {
    protected:
        control_base *manager_;

        bool read_8bit_data(const vm_address addr, std::uint8_t *data);
        bool read_16bit_data(const vm_address addr, std::uint16_t *data);
        bool read_32bit_data(const vm_address addr, std::uint32_t *data);
        bool read_64bit_data(const vm_address addr, std::uint64_t *data);

        bool write_8bit_data(const vm_address addr, std::uint8_t *data);
        bool write_16bit_data(const vm_address addr, std::uint16_t *data);
        bool write_32bit_data(const vm_address addr, std::uint32_t *data);
        bool write_64bit_data(const vm_address addr, std::uint64_t *data);

    public:
        arm::core *cpu_;
        config::state *conf_;

    public:
        explicit mmu_base(control_base *manager, arm::core *cpu, config::state *conf);
        virtual ~mmu_base() {}

        void map_to_cpu(const vm_address addr, const std::size_t size, void *ptr, const prot perm);
        void unmap_from_cpu(const vm_address addr, const std::size_t size);

        /**
         * \brief Set current MMU's address space.
         * 
         * \param id The ASID of the target address space
         */
        virtual bool set_current_addr_space(const asid id) = 0;

        virtual const asid current_addr_space() const = 0;
    };
}
