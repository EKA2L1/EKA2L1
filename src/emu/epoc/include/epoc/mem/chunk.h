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

#include <common/types.h>
#include <epoc/mem/common.h>

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

        virtual ~mem_model_chunk() {
        }

        virtual int do_create(const mem_model_chunk_creation_info &create_info) = 0;

        /**
         * \brief Find uncommitted region in the disconnected chunk with the specified size,
         *        and commit them.
         * 
         * \param size The size to find and commit. Round to page size.
         * 
         * \returns True on success.
         */
        virtual bool allocate(const std::size_t size) = 0;
        virtual bool adjust(const address bottom, const address top) = 0;

        virtual const vm_address bottom() const = 0;
        virtual const vm_address top() const = 0;
        virtual const std::size_t committed() const = 0;
        virtual const std::size_t max() const = 0;

        virtual const vm_address base() = 0;
        virtual std::size_t commit(const vm_address offset, const std::size_t size) = 0;
        virtual void decommit(const vm_address offset, const std::size_t size) = 0;

        virtual void *host_base() = 0;

        /**
         * \brief Unmap the committed chunk region from the CPU.
         * 
         * This is support for some JIT's context switching.
         */
        virtual void unmap_from_cpu() = 0;

        /**
         * \brief Map the committed region to CPU.
         * 
         * This is support for some JIT's context switching.
         */
        virtual void map_to_cpu() = 0;
    };

    using mem_model_chunk_impl = std::unique_ptr<mem_model_chunk>;

    mem_model_chunk_impl make_new_mem_model_chunk(mmu_base *mmu, const asid addr_space_id,
        const mem_model_type mmt);
}
