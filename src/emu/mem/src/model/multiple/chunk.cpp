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

#include <mem/model/multiple/chunk.h>
#include <mem/model/multiple/control.h>
#include <mem/model/multiple/process.h>

#include <common/algorithm.h>
#include <common/virtualmem.h>

#include <common/log.h>
#include <cpu/arm_interface.h>

namespace eka2l1::mem {
    std::size_t multiple_mem_model_chunk::commit(const vm_address offset, const std::size_t size, bool ignore_committed) {
        // Align the offset
        vm_address running_offset = offset;
        vm_address end_offset = common::min(static_cast<vm_address>(max_size_),
            static_cast<vm_address>(offset + size));

        while (running_offset < end_offset) {
            // The number of page sastify the request
            int page_num = (end_offset - running_offset) >> control_->page_size_bits_;

            const vm_address mapping_offset = (base_ & control_->chunk_mask_) + running_offset;
            const int page_num_till_end_pt = (control_->chunk_size_ - (mapping_offset & control_->chunk_mask_)) >> control_->page_size_bits_;

            page_num = common::min(page_num, page_num_till_end_pt);

            const auto table_index = mapping_offset >> control_->chunk_shift_;
            std::uint32_t ptid = page_tabs_[table_index];
            page_table *pt = nullptr;

            if (ptid == 0xFFFFFFFF) {
                // Small code segments share a page table instead of reserving one each.
                if (is_code && !control_->mem_map_old_) {
                    auto mul_ctrl = static_cast<control_multiple *>(control_);
                    pt = mul_ctrl->global_dir_.get_page_table(base_ + running_offset);
                }
                if (!pt) {
                    pt = control_->create_new_page_table();
                }
            } else {
                pt = control_->get_page_table_by_id(ptid);
            }

            // Start offset
            int ps_off = (mapping_offset >> control_->page_index_shift_) & control_->page_index_mask_;
            const auto psize = control_->page_size();

            std::size_t size_just_mapped = 0;
            vm_address off_start_just_mapped = 0;
            std::uint8_t *host_start_just_mapped = nullptr;

            std::uint8_t *host_commit_ptr = reinterpret_cast<std::uint8_t *>(host_base_) + running_offset;
            const std::size_t host_commit_size = page_num << control_->page_size_bits_;

            // Commit the memory to the host
            if (!is_external_host && !common::commit(host_commit_ptr, host_commit_size, permission_)) {
                return running_offset - offset;
            }

            const vm_address crr_base_addr = base_;

            multiple_mem_model_process *mul_process = reinterpret_cast<multiple_mem_model_process *>(own_process_);
            control_multiple *mul_ctrl = reinterpret_cast<control_multiple *>(control_);

            // Fill the entry
            for (int poff = ps_off; poff < ps_off + page_num; poff++) {
                // If the entry has not yet been committed.
                if (pt->pages_[poff].host_addr == nullptr) {
                    pt->pages_[poff].host_addr = host_commit_ptr + ((poff - ps_off) << control_->page_size_bits_);
                    pt->pages_[poff].perm = permission_;

                    // Increase committed size.
                    committed_ += psize;
                    size_just_mapped += psize;

                    if (off_start_just_mapped == 0) {
                        off_start_just_mapped = crr_base_addr + running_offset + ((poff - ps_off) << control_->page_size_bits_);
                        host_start_just_mapped = host_commit_ptr + ((poff - ps_off) << control_->page_size_bits_);
                    }
                } else {
                    if (!ignore_committed) {
                        LOG_TRACE(KERNEL, "Debug");
                        return static_cast<std::size_t>(-1);
                    }
                    // Map those just mapped to the CPU. It will love this
                    if (size_just_mapped != 0) {
                        for (auto &mm : mul_ctrl->mmus_) {
                            if (!own_process_ || mul_process->addr_space_id_ == mm->current_addr_space()) {
                                mm->map_to_cpu(off_start_just_mapped, size_just_mapped, host_start_just_mapped, permission_);
                                break;
                            }
                        }

                        off_start_just_mapped = 0;
                        size_just_mapped = 0;
                        host_start_just_mapped = nullptr;
                    }
                }
            }

            // Map the rest
            if (size_just_mapped != 0) {
                for (auto &mm : mul_ctrl->mmus_) {
                    if (!own_process_ || mul_process->addr_space_id_ == mm->current_addr_space()) {
                        mm->map_to_cpu(off_start_just_mapped, size_just_mapped, host_start_just_mapped, permission_);
                        break;
                    }
                }
                //LOG_TRACE(MEMORY, "Mapped to CPU: 0x{:X}, size 0x{:X}", off_start_just_mapped, size_just_mapped);
            }

            if (ptid == 0xFFFFFFFF) {
                // Assign the new page table to the specified address
                control_->assign_page_table(pt, crr_base_addr + running_offset, !is_local ? MMU_ASSIGN_LOCAL_GLOBAL_REGION : 0,
                    is_local ? &mul_process->addr_space_id_ : nullptr, is_local ? 1 : 0);

                page_tabs_[table_index] = pt->id();
            }

            // Alloc in-house bits
            if (page_bma_) {
                page_bma_->force_fill(running_offset >> control_->page_size_bits_, page_num);
            }

            running_offset += (page_num << control_->page_size_bits_);
        }

        return running_offset - offset;
    }

    void multiple_mem_model_chunk::decommit(const vm_address offset, const std::size_t size) {
        // Align the offset
        vm_address running_offset = offset;
        vm_address end_offset = common::min(static_cast<vm_address>(max_size_),
            static_cast<vm_address>(offset + size));

        while (running_offset < end_offset) {
            // The number of page sastify the request
            int page_num = (end_offset - running_offset) >> control_->page_size_bits_;

            const vm_address mapping_offset = (base_ & control_->chunk_mask_) + running_offset;
            const int page_num_till_end_pt = (control_->chunk_size_ - (mapping_offset & control_->chunk_mask_)) >> control_->page_size_bits_;

            page_num = common::min(page_num, page_num_till_end_pt);
            const auto table_index = mapping_offset >> control_->chunk_shift_;
            std::uint32_t ptid = page_tabs_[table_index];
            page_table *pt = nullptr;

            if (ptid == 0xFFFFFFFF) {
                running_offset += (page_num << control_->page_size_bits_);
                continue;
            } else {
                pt = control_->get_page_table_by_id(ptid);
            }

            // Start offset
            int ps_off = (mapping_offset >> control_->page_index_shift_) & control_->page_index_mask_;
            const auto psize = control_->page_size();

            std::size_t size_just_unmapped = 0;
            vm_address off_start_just_unmapped = 0;

            const vm_address crr_base_addr = base_;

            control_multiple *mul_ctrl = reinterpret_cast<control_multiple *>(control_);

            // Fill the entry
            for (int poff = ps_off; poff < ps_off + page_num; poff++) {
                // If the entry has not yet been committed.
                if (pt->pages_[poff].host_addr != nullptr) {
                    pt->pages_[poff].host_addr = nullptr;

                    // Increase committed size.
                    committed_ -= psize;
                    size_just_unmapped += psize;

                    if (off_start_just_unmapped == 0) {
                        off_start_just_unmapped = crr_base_addr + running_offset + ((poff - ps_off) << control_->page_size_bits_);
                    }
                } else {
                    // Invalidate the CPU TLB unconditionally: globally-visible pages
                    // (shared/global sections, code) may sit in the running core's TLB
                    // even when the chunk's owner is not the current address space, and
                    // the host memory is about to be freed. Dirtying a stale-tagged or
                    // foreign entry is always safe (it just re-resolves on next access).
                    if (size_just_unmapped != 0) {
                        // Use linear loop since the size is expected to be small
                        for (auto &mm : mul_ctrl->mmus_) {
                            mm->unmap_from_cpu(off_start_just_unmapped, size_just_unmapped);
                        }

                        size_just_unmapped = 0;
                        off_start_just_unmapped = 0;
                    }
                }
            }

            // Unmap the rest
            if (size_just_unmapped != 0) {
                //LOG_TRACE(MEMORY, "Unmapped from CPU: 0x{:X}, size 0x{:X}", off_start_just_unmapped, size_just_unmapped);
                for (auto &mm : mul_ctrl->mmus_) {
                    mm->unmap_from_cpu(off_start_just_unmapped, size_just_unmapped);
                }
            }

            // Decommit the memory from the host
            if (!is_external_host) {
                if (!common::decommit(reinterpret_cast<std::uint8_t *>(host_base_) + running_offset,
                        page_num << control_->page_size_bits_)) {
                    LOG_ERROR(MEMORY, "Can't decommit a page from host memory");
                }
            }

            // Dealloc in-house bits
            if (page_bma_) {
                page_bma_->force_fill(running_offset >> control_->page_size_bits_, page_num, true);
            }

            running_offset += (page_num << control_->page_size_bits_);
        }
    }

    std::int32_t multiple_mem_model_chunk::allocate(const std::size_t size) {
        if (!page_bma_) {
            return -1;
        }

        // Actual size
        const std::size_t num_pages = ((size + control_->page_size() - 1) >> control_->page_size_bits_);
        const std::size_t size_pages = num_pages << control_->page_size_bits_;

        // Allocate space that hasn't been allocated yet
        int num_pages_i = static_cast<int>(num_pages);
        const int off = page_bma_->allocate_from(0, num_pages_i, true);

        if (off == -1) {
            return -1;
        }

        commit(off << control_->page_size_bits_, size_pages);
        return static_cast<std::int32_t>(off << control_->page_size_bits_);
    }

    linear_section *multiple_mem_model_chunk::get_section(const std::uint32_t flags) {
        control_multiple *mul_mmu = reinterpret_cast<control_multiple *>(control_);
        multiple_mem_model_process *mul_process = reinterpret_cast<multiple_mem_model_process *>(own_process_);

        if (flags & MEM_MODEL_CHUNK_REGION_USER_CODE) {
            return &mul_mmu->user_code_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_GLOBAL) {
            return &mul_mmu->user_global_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) {
            return &mul_process->user_local_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_DLL_STATIC_DATA) {
            return &mul_process->user_dll_static_data_sec_;
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

        create_flags_ = create_info.flags;

        if ((create_info.flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) || (create_info.flags & MEM_MODEL_CHUNK_REGION_DLL_STATIC_DATA)) {
            is_local = true;
        } else {
            is_local = false;
        }

        is_code = false;

        if (create_info.flags & MEM_MODEL_CHUNK_REGION_USER_CODE) {
            is_code = true;
        }

        const std::uint32_t total_pt = static_cast<std::uint32_t>(
            (create_info.size + control_->chunk_mask_) >> control_->chunk_shift_);

        if (is_code && !control_->mem_map_old_) {
            granularity_shift_ = control_->page_size_bits_;
        }

        // Calculate the aligned max size, depends on the granularity.
        // The granularity is set in mem model process's chunk creation.
        const std::uint32_t total_granularity_block = static_cast<std::uint32_t>(
            (create_info.size + (1ULL << granularity_shift_) - 1) >> granularity_shift_);

        max_size_ = total_granularity_block << granularity_shift_;

        // Allocate virtual pages
        vm_address addr = create_info.addr;

        if (addr == 0) {
            const int requested_pages = (is_code && !control_->mem_map_old_)
                ? static_cast<int>(max_size_ >> control_->page_size_bits_)
                : static_cast<int>(total_pt << control_->page_per_tab_shift_);
            int max_page = requested_pages;
            const int offset = chunk_sec->alloc_.allocate_from(0, max_page, false);

            if (offset == -1 || max_page != requested_pages) {
                if (offset != -1) {
                    chunk_sec->alloc_.deallocate(offset, max_page);
                }
                max_size_ = 0;
                return MEM_MODEL_CHUNK_ERR_NO_MEM;
            }

            addr = (offset << control_->page_size_bits_) + chunk_sec->beg_;
            max_size_ = static_cast<std::size_t>(max_page) << control_->page_size_bits_;
        } else {
            addr &= ~(((1 << control_->page_per_tab_shift_) << control_->page_size_bits_) - 1);

            const std::uint32_t total_page = total_pt << control_->page_per_tab_shift_;
            const std::size_t total_page_size = static_cast<std::size_t>(total_page) << control_->page_size_bits_;

            // Forced ROM chunks may live outside the allocator's section.
            if ((addr >= chunk_sec->beg_) && (static_cast<std::uint64_t>(addr) + total_page_size <= chunk_sec->end_)) {
                // Mark those as allocated
                max_size_ = chunk_sec->alloc_.force_fill((addr - chunk_sec->beg_) >> control_->page_size_bits_,
                    static_cast<int>(total_page), false);

                max_size_ <<= control_->page_size_bits_;
            } else {
                max_size_ = total_page_size;
            }

            create_flags_ |= MEM_MODEL_CHUNK_INTERNAL_FORCE_FILL;
        }

        base_ = addr;
        committed_ = 0;

        page_tabs_.resize(((base_ & control_->chunk_mask_) + max_size_ + control_->chunk_mask_) >> control_->chunk_shift_);
        std::fill(page_tabs_.begin(), page_tabs_.end(), 0xFFFFFFFF);

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

        permission_ = create_info.perm;

        if (create_info.flags & MEM_MODEL_CHUNK_TYPE_DISCONNECT) {
            page_bma_ = std::make_unique<common::bitmap_allocator>(max_size_ >> control_->page_size_bits_);
        }

        return MEM_MODEL_CHUNK_ERR_OK;
    }

    void multiple_mem_model_chunk::do_selection_cpu_memory_manipulation(mmu_base *mmu, const bool unmap) {
        manipulate_cpu_map(page_bma_.get(), nullptr, mmu, !unmap);
    }

    void multiple_mem_model_chunk::unmap_from_cpu(mem_model_process *pr, mmu_base *mmu) {
        do_selection_cpu_memory_manipulation(mmu, true);
    }

    void multiple_mem_model_chunk::map_to_cpu(mem_model_process *pr, mmu_base *mmu) {
        do_selection_cpu_memory_manipulation(mmu, false);
    }

    multiple_mem_model_chunk::~multiple_mem_model_chunk() {
        if (max_size_ == 0) {
            return;
        }

        // Decommit the whole things
        decommit(0, max_size_);

        // Free the region that previously allocated from the allocator
        if (!(create_flags_ & MEM_MODEL_CHUNK_INTERNAL_FORCE_FILL)) {
            linear_section *sec = get_section(create_flags_);

            if (sec)
                sec->alloc_.deallocate((base_ - sec->beg_) >> control_->page_size_bits_, max_size_ >> control_->page_size_bits_);
        }

        if (is_code) {
            control_multiple *control_mm = reinterpret_cast<control_multiple *>(control_);
            for (auto &mmu : control_mm->mmus_) {
                // Clear the instruction cache at that range, code may reuse it later
                mmu->cpu_->imb_range(base_, max_size_);
            }
        }

        // Ignore the result, just unmap things
        if (!is_external_host)
            common::unmap_memory(host_base_, max_size_);
    }
}
