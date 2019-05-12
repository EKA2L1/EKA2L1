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

#include <epoc/mem/chunk.h>
#include <vector>

namespace eka2l1::mem {
    struct multiple_mem_model_process;

    struct multiple_mem_model_chunk: public mem_model_chunk {
        vm_address base_;
        void *host_base_;

        multiple_mem_model_process *own_process_;

        std::size_t chunk_id_in_mmp_;
        vm_address bottom_;
        vm_address top_;

        std::size_t committed_;
        std::size_t max_size_;

        std::vector<std::uint32_t> page_tabs_;
        std::vector<asid> attached_asids_;

    public:
        explicit multiple_mem_model_chunk(mmu_base *mmu, const asid id)
            : mem_model_chunk(mmu, id) {
        }

        const vm_address base() override {
            return base_;
        }

        std::size_t commit(const vm_address offset, const std::size_t size) override;
        void decommit(const vm_address offset, const std::size_t size) override;
    };
}
