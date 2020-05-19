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
#include <epoc/mem/model/multiple/process.h>

#include <common/algorithm.h>
#include <common/virtualmem.h>

#include <common/log.h>

namespace eka2l1::mem {
    const vm_address multiple_mem_model_chunk::bottom() const {
        return bottom_ << mmu_->page_size_bits_;
    }

    const vm_address multiple_mem_model_chunk::top() const {
        return top_ << mmu_->page_size_bits_;
    }

    std::size_t multiple_mem_model_chunk::commit(const vm_address offset, const std::size_t size) {
        // Align the offset
        vm_address running_offset = offset;
        vm_address end_offset = common::min(static_cast<vm_address>(max_size_),
            static_cast<vm_address>(offset + size));

        while (running_offset < end_offset) {
            // The number of page sastify the request
            int page_num = (end_offset - running_offset) >> mmu_->page_size_bits_;

            // The number of page to the end of page table
            const int page_num_till_end_pt = (mmu_->chunk_size_ - (running_offset & mmu_->chunk_mask_)) >> mmu_->page_size_bits_;

            page_num = common::min(page_num, page_num_till_end_pt);

            std::uint32_t ptid = page_tabs_[running_offset >> mmu_->chunk_shift_];
            page_table *pt = nullptr;

            if (ptid == 0xFFFFFFFF) {
                pt = mmu_->create_new_page_table();
            } else {
                pt = mmu_->get_page_table_by_id(ptid);
            }

            // Start offset
            int ps_off = (running_offset >> mmu_->page_index_shift_) & mmu_->page_index_mask_;
            const auto psize = mmu_->page_size();

            std::size_t size_just_mapped = 0;
            vm_address off_start_just_mapped = 0;
            std::uint8_t *host_start_just_mapped = nullptr;

            const auto pt_base = (running_offset >> mmu_->chunk_shift_) << mmu_->chunk_shift_;

            // Commit the memory to the host
            if (!is_external_host && !common::commit(reinterpret_cast<std::uint8_t *>(host_base_) + (ps_off << mmu_->page_size_bits_) + pt_base, page_num << mmu_->page_size_bits_, permission_)) {
                return running_offset - offset;
            }

            // Fill the entry
            for (int poff = ps_off; poff < ps_off + page_num; poff++) {
                // If the entry has not yet been committed.
                if (pt->pages_[poff].host_addr == nullptr) {
                    pt->pages_[poff].host_addr = reinterpret_cast<std::uint8_t *>(host_base_) + (poff << mmu_->page_size_bits_) + pt_base;
                    pt->pages_[poff].perm = permission_;

                    // Increase committed size.
                    committed_ += psize;
                    size_just_mapped += psize;

                    if (off_start_just_mapped == 0) {
                        off_start_just_mapped = (poff << mmu_->page_size_bits_) + base_ + pt_base;
                        host_start_just_mapped = reinterpret_cast<std::uint8_t *>(host_base_) + (poff << mmu_->page_size_bits_) + pt_base;
                    }
                } else {
                    // Map those just mapped to the CPU. It will love this
                    if (size_just_mapped != 0 && (!own_process_ || own_process_->addr_space_id_ == mmu_->current_addr_space())) {
                        mmu_->map_to_cpu(off_start_just_mapped, size_just_mapped, host_start_just_mapped, permission_);
                        off_start_just_mapped = 0;
                        size_just_mapped = 0;
                        host_start_just_mapped = nullptr;
                    }
                }
            }

            // Map the rest
            if (size_just_mapped != 0 && (!own_process_ || own_process_->addr_space_id_ == mmu_->current_addr_space())) {
                //LOG_TRACE("Mapped to CPU: 0x{:X}, size 0x{:X}", off_start_just_mapped, size_just_mapped);
                mmu_->map_to_cpu(off_start_just_mapped, size_just_mapped, host_start_just_mapped, permission_);
            }

            if (ptid == 0xFFFFFFFF) {
                // Assign the new page table to the specified address
                mmu_->assign_page_table(pt, base_ + pt_base, !is_local ? MMU_ASSIGN_LOCAL_GLOBAL_REGION : 0,
                    is_local ? &own_process_->addr_space_id_ : nullptr, is_local ? 1 : 0);

                page_tabs_[running_offset >> mmu_->chunk_shift_] = pt->id();
            }

            // Alloc in-house bits
            if (page_bma_) {
                page_bma_->force_fill(running_offset >> mmu_->page_size_bits_, page_num);
            }

            running_offset += (page_num << mmu_->page_size_bits_);
        }

        return running_offset - offset;
    }

    void multiple_mem_model_chunk::decommit(const vm_address offset, const std::size_t size) {
        // Align the offset
        vm_address running_offset = offset;
        vm_address end_offset = common::min(static_cast<vm_address>(offset + max_size_),
            static_cast<vm_address>(offset + size));

        while (running_offset < end_offset) {
            // The number of page sastify the request
            int page_num = (end_offset - running_offset) >> mmu_->page_size_bits_;

            // The number of page to the end of page table
            const int page_num_till_end_pt = (mmu_->chunk_size_ - (running_offset & mmu_->chunk_mask_)) >> mmu_->page_size_bits_;

            page_num = common::min(page_num, page_num_till_end_pt);
            std::uint32_t ptid = page_tabs_[running_offset >> mmu_->chunk_shift_];
            page_table *pt = nullptr;

            if (ptid == 0xFFFFFFFF) {
                running_offset += (page_num << mmu_->page_size_bits_);
                continue;
            } else {
                pt = mmu_->get_page_table_by_id(ptid);
            }

            // Start offset
            int ps_off = (running_offset >> mmu_->page_index_shift_) & mmu_->page_index_mask_;
            const auto psize = mmu_->page_size();

            std::size_t size_just_unmapped = 0;
            vm_address off_start_just_unmapped = 0;

            const auto pt_base = (running_offset >> mmu_->chunk_shift_) << mmu_->chunk_shift_;

            // Fill the entry
            for (int poff = ps_off; poff < ps_off + page_num; poff++) {
                // If the entry has not yet been committed.
                if (pt->pages_[poff].host_addr != nullptr) {
                    pt->pages_[poff].host_addr = nullptr;

                    // Increase committed size.
                    committed_ -= psize;
                    size_just_unmapped += psize;

                    if (off_start_just_unmapped == 0) {
                        off_start_just_unmapped = (poff << mmu_->page_size_bits_) + base_ + pt_base;
                    }
                } else {
                    // Map those just mapped to the CPU. It will love this
                    if (size_just_unmapped != 0 && (!own_process_ || own_process_->addr_space_id_ == mmu_->current_addr_space())) {
                        mmu_->unmap_from_cpu(off_start_just_unmapped, size_just_unmapped);

                        size_just_unmapped = 0;
                        off_start_just_unmapped = 0;
                    }
                }
            }

            // Unmap the rest
            if (size_just_unmapped != 0 && (!own_process_ || own_process_->addr_space_id_ == mmu_->current_addr_space())) {
                //LOG_TRACE("Unmapped from CPU: 0x{:X}, size 0x{:X}", off_start_just_unmapped, size_just_unmapped);
                mmu_->unmap_from_cpu(off_start_just_unmapped, size_just_unmapped);
            }

            // Decommit the memory from the host
            if (!is_external_host) {
                if (!common::decommit(reinterpret_cast<std::uint8_t *>(host_base_) + (ps_off << mmu_->page_size_bits_) + pt_base,
                        page_num << mmu_->page_size_bits_)) {
                    LOG_ERROR("Can't decommit a page from host memory");
                }
            }

            // Dealloc in-house bits
            if (page_bma_) {
                page_bma_->force_fill(running_offset >> mmu_->page_size_bits_, page_num, true);
            }

            running_offset += (page_num << mmu_->page_size_bits_);
        }
    }

    bool multiple_mem_model_chunk::allocate(const std::size_t size) {
        if (!page_bma_) {
            return false;
        }

        // Actual size
        const std::size_t num_pages = ((size + mmu_->page_size() - 1) >> mmu_->page_size_bits_);
        const std::size_t size_pages = num_pages << mmu_->page_size_bits_;

        // Allocate space that hasn't been allocated yet
        int num_pages_i = static_cast<int>(num_pages);
        const int off = page_bma_->allocate_from(0, num_pages_i, true);

        if (off == -1) {
            return false;
        }

        return commit(off << mmu_->page_size_bits_, size_pages);
    }

    bool multiple_mem_model_chunk::adjust(const address bottom, const address top) {
        const std::size_t top_page_off = ((top + mmu_->page_size() - 1) >> mmu_->page_size_bits_);
        const std::size_t bottom_page_off = (bottom >> mmu_->page_size_bits_);

        // Check the top
        // Top offset adjusted smaller than current top offset
        if (top_page_off < top_) {
            // Decommit
            decommit(static_cast<vm_address>(top_page_off << mmu_->page_size_bits_),
                (top_ - top_page_off) << mmu_->page_size_bits_);
        } else if (top_page_off > top_) {
            // We must commit more memory
            commit(static_cast<vm_address>(top_ << mmu_->page_size_bits_),
                (top_page_off - top_) << mmu_->page_size_bits_);
        }

        top_ = static_cast<vm_address>(top_page_off);

        // Adjust double ended
        if (bottom != 0xFFFFFFFF) {
            // Check the bottom
            if (bottom_page_off > bottom_) {
                // Decommit
                decommit(static_cast<vm_address>(bottom_ << mmu_->page_size_bits_),
                    (bottom_page_off - bottom_) << mmu_->page_size_bits_);
            } else if (bottom_page_off < bottom_) {
                // We must commit more memory
                commit(static_cast<vm_address>(bottom_page_off << mmu_->page_size_bits_),
                    (bottom_ - bottom_page_off) << mmu_->page_size_bits_);
            }

            bottom_ = static_cast<vm_address>(bottom_page_off);
        }

        return true;
    }

    linear_section *multiple_mem_model_chunk::get_section(const std::uint32_t flags) {
        mmu_multiple *mul_mmu = reinterpret_cast<mmu_multiple *>(mmu_);

        if (flags & MEM_MODEL_CHUNK_REGION_USER_CODE) {
            return &mul_mmu->user_code_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_GLOBAL) {
            return &mul_mmu->user_global_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) {
            return &own_process_->user_local_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_ROM) {
            return &mul_mmu->user_rom_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_KERNEL_MAPPING) {
            return &mul_mmu->kernel_mapping_sec_;
        }

        return nullptr;
    }

    int multiple_mem_model_chunk::do_create(const mem_model_chunk_creation_info &create_info) {
        // get the right virtual address space allocator
        linear_section *chunk_sec = get_section(create_info.flags);

        if (!chunk_sec) {
            // No allocator suitable for the given flags
            return MEM_MODEL_CHUNK_ERR_INVALID_REGION;
        }

        if (create_info.flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) {
            is_local = true;
        } else {
            is_local = false;
        }

        // Calculate total page table that we will use. Symbian uses a whole page table
        // too, it's one of a way for it to be fast (not nitpicking places in the table,
        // but actually alloc the whole table)
        const std::uint32_t total_pt = static_cast<std::uint32_t>(
            (create_info.size + mmu_->chunk_mask_) >> mmu_->chunk_shift_);

        max_size_ = total_pt << mmu_->chunk_shift_;

        // Allocate virtual pages
        vm_address addr = create_info.addr;

        if (addr == 0) {
            int max_page = total_pt << mmu_->page_per_tab_shift_;
            const int offset = chunk_sec->alloc_.allocate_from(0, max_page, false);

            if (offset == -1) {
                return MEM_MODEL_CHUNK_ERR_NO_MEM;
            }

            addr = (offset << mmu_->page_size_bits_) + chunk_sec->beg_;
            max_size_ = static_cast<std::size_t>(max_page) << mmu_->page_size_bits_;
        } else {
            // Mark those as allocated
            max_size_ = chunk_sec->alloc_.force_fill((addr - chunk_sec->beg_) >> mmu_->page_size_bits_,
                static_cast<int>(total_pt << mmu_->page_per_tab_shift_), false);

            max_size_ <<= mmu_->page_size_bits_;
        }

        base_ = addr;
        committed_ = 0;

        // Map host base memory
        if (create_info.host_map) {
            host_base_ = create_info.host_map;
            is_external_host = true;
        } else {
            host_base_ = common::map_memory(max_size_);
            is_external_host = false;
        }

        if (!host_base_) {
            return MEM_MODEL_CHUNK_ERR_NO_MEM;
        }

        // Allocate page table IDs storage that the chunk will use
        page_tabs_.resize(total_pt);
        std::fill(page_tabs_.begin(), page_tabs_.end(), 0xFFFFFFFF);

        permission_ = create_info.perm;

        if (create_info.flags & MEM_MODEL_CHUNK_TYPE_DISCONNECT) {
            page_bma_ = std::make_unique<common::bitmap_allocator>(max_size_ >> mmu_->page_size_bits_);
        }

        return MEM_MODEL_CHUNK_ERR_OK;
    }

    void multiple_mem_model_chunk::do_selection_cpu_memory_manipulation(const bool unmap) {
        if (!page_bma_) {
            // Contiguous types. Just unmap/map directly
            if (unmap) {
                mmu_->unmap_from_cpu(base_ + (bottom_ << mmu_->page_size_bits_), (top_ - bottom_) << mmu_->page_size_bits_);
            } else {
                mmu_->map_to_cpu(base_ + (bottom_ << mmu_->page_size_bits_), (top_ - bottom_) << mmu_->page_size_bits_,
                    reinterpret_cast<std::uint8_t *>(host_base_) + (bottom_ << mmu_->page_size_bits_), permission_);
            }

            return;
        }

        // Iterates through all pages, it's disconnected
        vm_address running_offset = 0;
        vm_address end_offset = static_cast<vm_address>(max_size_);

        while (running_offset < end_offset) {
            // The number of page satisfy the request
            int page_num = (end_offset - running_offset) >> mmu_->page_size_bits_;

            // The number of page to the end of page table
            const int page_num_till_end_pt = (mmu_->chunk_size_ - (running_offset & mmu_->chunk_mask_)) >> mmu_->page_size_bits_;

            page_num = common::min(page_num, page_num_till_end_pt);

            std::uint32_t ptid = page_tabs_[running_offset >> mmu_->chunk_shift_];
            page_table *pt = nullptr;

            if (ptid == 0xFFFFFFFF) {
                // Page table is not here. Skip....
                running_offset += (page_num << mmu_->page_size_bits_);
                continue;
            } else {
                pt = mmu_->get_page_table_by_id(ptid);
            }

            // Start offset
            int ps_off = (running_offset >> mmu_->page_index_shift_) & mmu_->page_index_mask_;
            const auto psize = mmu_->page_size();

            std::size_t size_mani = 0;
            vm_address off_start_mani = 0;
            std::uint8_t *host_start_mani = nullptr;

            const auto pt_base = (running_offset >> mmu_->chunk_shift_) << mmu_->chunk_shift_;

            // Fill the entry
            for (int poff = ps_off; poff < ps_off + page_num; poff++) {
                if (pt->pages_[poff].host_addr != nullptr) {
                    size_mani += psize;

                    if (off_start_mani == 0) {
                        off_start_mani = (poff << mmu_->page_size_bits_) + base_ + pt_base;
                        host_start_mani = reinterpret_cast<std::uint8_t *>(host_base_) + (poff << mmu_->page_size_bits_) + pt_base;
                    }
                } else {
                    // Map those just mapped to the CPU. It will love this
                    if (size_mani != 0) {
                        if (unmap) {
                            mmu_->unmap_from_cpu(off_start_mani, size_mani);
                        } else {
                            mmu_->map_to_cpu(off_start_mani, size_mani, host_start_mani, permission_);
                        }
                    }
                }
            }

            // Map those just mapped to the CPU. It will love this
            if (size_mani != 0) {
                if (unmap) {
                    mmu_->unmap_from_cpu(off_start_mani, size_mani);
                } else {
                    mmu_->map_to_cpu(off_start_mani, size_mani, host_start_mani, permission_);
                }
            }

            running_offset += (page_num << mmu_->page_size_bits_);
        }
    }

    void multiple_mem_model_chunk::unmap_from_cpu() {
        do_selection_cpu_memory_manipulation(true);
    }

    void multiple_mem_model_chunk::map_to_cpu() {
        do_selection_cpu_memory_manipulation(false);
    }
}
