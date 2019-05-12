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

#include <epoc/mem/common.h>
#include <common/types.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace eka2l1::mem {
    class mmu_base;

    struct mem_model_chunk {
        prot permission_;
    protected:
        mmu_base *mmu_;
        asid addr_space_id_;

    public:
        explicit mem_model_chunk(mmu_base *mmu, const asid id)
            : mmu_(mmu)
            , addr_space_id_(id) {
        }

        virtual const vm_address base() = 0;
        virtual std::size_t commit(const vm_address offset, const std::size_t size) = 0;
        virtual void decommit(const vm_address offset, const std::size_t size) = 0;
    };

    using mem_model_chunk_impl = std::unique_ptr<mem_model_chunk>;

    mem_model_chunk_impl make_new_mem_model_chunk(mmu_base *mmu, const asid addr_space_id,
        const mem_model_type mmt);
}
