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

#include <mem/mmu.h>
#include <mem/model/section.h>

#include <memory>

namespace eka2l1::mem {
    /**
     * \brief Memory management unit for multiple model.
     */
    class mmu_multiple : public mmu_base {
    protected:
        page_directory *cur_dir_;

    public:
        explicit mmu_multiple(control_base *manager, arm::core *cpu, config::state *conf);
        ~mmu_multiple() override {}

        void *get_host_pointer(const vm_address addr) override;

        const asid current_addr_space() const override;
        bool set_current_addr_space(const asid id) override;

        page_table *get_page_table_by_addr(const vm_address addr);
    };
}
