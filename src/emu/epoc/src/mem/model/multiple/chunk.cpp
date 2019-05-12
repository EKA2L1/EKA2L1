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

#include <epoc/mem/model/multiple/chunk.h>
#include <epoc/mem/model/multiple/mmu.h>

#include <common/algorithm.h>
#include <common/virtualmem.h>

namespace eka2l1::mem {
    std::size_t multiple_mem_model_chunk::commit(const vm_address offset, const std::size_t size) {
        // Align the offset
        vm_address running_offset = offset;
        vm_address end_offset = static_cast<vm_address>(offset + size);

        while (running_offset < end_offset) {
            // The number of page sastify the request
            int page_num = (end_offset - running_offset) >> mmu_->page_size_bits_;

            // The number of page to the end of page table
            const int page_num_till_end_pt = (mmu_->chunk_size_ - (running_offset & mmu_->chunk_mask_)) >> 
                mmu_->page_size_bits_;

            page_num = common::min(page_num, page_num_till_end_pt);

            std::uint32_t ptid = page_tabs_[running_offset >> mmu_->chunk_shift_];
            page_table *pt = nullptr;

            if (ptid == 0xFFFFFFFF) {
                pt = mmu_->create_new_page_table();
            } else {
                pt = mmu_->get_page_table_by_id(ptid);
            }

            // Start offset
            int ps_off = running_offset >> mmu_->page_index_shift_;
            const auto psize = mmu_->page_size();

            std::size_t size_just_mapped = 0;
            vm_address off_start_just_mapped = 0;
            std::uint8_t *host_start_just_mapped = nullptr;

            // Commit the memory to the host
            if (!common::commit(reinterpret_cast<std::uint8_t*>(host_base_) + (ps_off << mmu_->page_size_bits_),
                page_num << mmu_->page_size_bits_, permission_)) {
                return 0;
            }

            // Fill the entry
            for (int poff = ps_off; poff < ps_off + page_num; poff++) {
                // If the entry has not yet been committed.
                if (pt->pages_[poff].host_addr == nullptr) {
                    pt->pages_[poff].host_addr = reinterpret_cast<std::uint8_t*>(host_base_) + (poff << mmu_->page_size_bits_);
                    pt->pages_[poff].perm = permission_;

                    // Increase committed size.
                    committed_ += psize;
                    size_just_mapped += psize;

                    if (off_start_just_mapped == 0) {
                        off_start_just_mapped = (poff << mmu_->page_size_bits_) + base_;
                        host_start_just_mapped =  reinterpret_cast<std::uint8_t*>(host_base_) + (poff << mmu_->page_size_bits_);
                    }
                } else {
                    // Map those just mapped to the CPU. It will love this
                    if (size_just_mapped != 0) {
                        mmu_->map_to_cpu(off_start_just_mapped, size_just_mapped, host_start_just_mapped, permission_);
                    }
                }
            }

            // Map the rest
            if (size_just_mapped != 0) {
                mmu_->map_to_cpu(off_start_just_mapped, size_just_mapped, host_start_just_mapped, permission_);
            }

            if (ptid == 0xFFFFFFFF) {
                // Assign the new page table to the specified address
                mmu_->assign_page_table(pt, (ps_off << mmu_->page_size_bits_) + base_, own_process_ ? MMU_ASSIGN_LOCAL_GLOBAL_REGION : 0);
                page_tabs_[running_offset >> mmu_->chunk_shift_] = pt->id();
            }

            running_offset += (page_num << mmu_->page_size_bits_);
        }

        return 0;
    }

    void multiple_mem_model_chunk::decommit(const vm_address offset, const std::size_t size) {
        return;
    }
}
