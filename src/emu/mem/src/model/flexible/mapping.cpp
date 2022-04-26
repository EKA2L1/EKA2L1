/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <mem/model/flexible/addrspace.h>
#include <mem/model/flexible/control.h>
#include <mem/model/flexible/mapping.h>
#include <mem/model/flexible/memobj.h>
#include <mem/model/flexible/pagearray.h>

#include <common/log.h>
#include <common/algorithm.h>

namespace eka2l1::mem::flexible {
    mapping::mapping(address_space *owner)
        : owner_(owner) {
    }

    mapping::~mapping() {
        // Unmap all memory mapped
        unmap(0, occupied_);
    }

    bool mapping::instantiate(const std::size_t page_occupied, const std::uint32_t flags, const vm_address forced) {
        // Try to get linear section from our mommy first <3
        linear_section *sect = owner_->section(flags);

        if (!sect) {
            LOG_ERROR(MEMORY, "Instantiate new mapping failed because section with this flags not found: 0x{:X}", flags);
            return false;
        }

        int total_page = static_cast<int>(page_occupied);

        if (!forced) {
            const int offset = sect->alloc_.allocate_from(0, total_page, false);

            if (offset == -1) {
                LOG_ERROR(MEMORY, "Unable to instantiate new mapping on ASID 0x{:X}, out of virtual memory", owner_->id());
                return false;
            }

            if (total_page != static_cast<int>(page_occupied)) {
                LOG_WARN(MEMORY, "Unable to allocate all pages given in the mapping instantiate parameter");
            }

            base_ = sect->beg_ + (offset << owner_->control_->page_size_bits_);
        } else {
            if (forced < sect->beg_) {
                LOG_ERROR(MEMORY, "The forced base address is higher than the section base!");
                return false;
            }

            // Allocate as a holder, prevent other from allocate at same address!
            base_ = forced;
            sect->alloc_.allocate_from((base_ - sect->beg_) >> owner_->control_->page_size_bits_, total_page, false);
        }

        occupied_ = total_page;

        // We are going to the prom! Yes, me, the mapping.
        owner_->mappings_.push_back(this);
        return true;
    }

    bool mapping::map(memory_object *obj, const std::uint32_t index, const std::size_t count, const prot permissions) {
        if (index + count > occupied_) {
            // Too far
            return false;
        }

        control_base *control = owner_->control_;

        const std::uint32_t start_offset = +(index << control->page_size_bits_);

        vm_address start_addr = base_ + start_offset;
        const vm_address end_addr = start_addr + static_cast<vm_address>(count << control->page_size_bits_);

        const std::size_t page_size = control->page_size();
        std::uint8_t *starting_point_host = reinterpret_cast<std::uint8_t *>(obj->ptr()) + start_offset;

        page_table *faulty = nullptr;

        while (start_addr < end_addr) {
            const std::uint32_t ptoff = start_addr >> control->page_table_index_shift_;

            std::uint32_t next_end_addr = ((ptoff + 1) << control->chunk_shift_);
            next_end_addr = std::min<std::uint32_t>(next_end_addr, end_addr);

            std::uint32_t start_page_index = (start_addr >> control->page_index_shift_);
            const std::uint32_t end_page_index = (next_end_addr >> control->page_index_shift_);

            // Try to get the page table from daddy
            page_table *tbl = owner_->dir_->get_page_table(start_addr);

            if (!tbl) {
                // Ask the MMU to create a new page table and assign it
                tbl = control->create_new_page_table();
                tbl->idx_ = ptoff;
                owner_->dir_->set_page_table(ptoff, tbl);
            }

            while (start_page_index < end_page_index) {
                page_info *info = tbl->get_page_info(start_page_index & control->page_index_mask_);
                if (info) {
                    info->host_addr = starting_point_host;
                    info->perm = permissions;
                }

                start_page_index++;
                starting_point_host += page_size;
            }

            start_addr = next_end_addr;
        }

        return true;
    }

    bool mapping::unmap(const std::uint32_t index_start, const std::size_t count) {
        if (index_start + count > occupied_) {
            // Too far
            return false;
        }

        control_base *control = owner_->control_;

        vm_address start_addr = base_ + (index_start << control->page_size_bits_);
        const vm_address end_addr = start_addr + static_cast<vm_address>(count << control->page_size_bits_);

        const std::size_t page_size = control->page_size();

        while (start_addr < end_addr) {
            const std::uint32_t ptoff = start_addr >> control->chunk_shift_;

            std::uint32_t next_end_addr = ((ptoff + 1) << control->chunk_shift_);
            next_end_addr = std::min<std::uint32_t>(next_end_addr, end_addr);

            std::uint32_t start_page_index = (start_addr >> control->page_index_shift_) & control->page_index_mask_;
            const std::uint32_t end_page_index = (next_end_addr >> control->page_index_shift_) & control->page_index_mask_;

            // Try to get the page table from daddy
            page_table *tbl = owner_->dir_->get_page_table(start_addr);

            if (tbl) {
                // Proceed with the unmapping
                while (start_page_index < end_page_index) {
                    page_info *info = tbl->get_page_info(start_page_index);
                    if (info) {
                        // Empty it out
                        info->host_addr = nullptr;
                    }

                    start_page_index++;
                }
            }

            start_addr = next_end_addr;
        }

        return true;
    }

    void pages_segment::supply_mapping(memory_object *obj, mapping *map, const std::uint32_t segment_index, const std::uint32_t limit) {
        std::uint32_t page_index_start = segment_index * TOTAL_PAGE_PER_SEGMENT;
        std::uint32_t qword_check = (limit + PAGE_PER_QWORD - 1) / PAGE_PER_QWORD;
        std::uint64_t prot_val = 0;
        std::uint64_t prot_temp = 0;
        std::uint32_t lim_check = 0;
        std::uint32_t count = 0;
        std::uint32_t start = 0;

        for (std::uint32_t i = 0; i < qword_check; i++) {
            std::uint32_t base = (i << PAGE_PER_QWORD_SHIFT);
            if (prots_[i] != 0) {
                std::uint32_t index = static_cast<std::uint32_t>(common::find_least_significant_bit_one(prots_[i]) << 2);
                if (index + base >= limit) {
                    return;
                }

                prot_val = (prots_[i] >> (index << 2)) & 0b1111;
                lim_check = common::min<std::uint32_t>(16U, limit - base);
                start = index;
                index++;
                count = 1;

                if (index == lim_check) {
                    map->map(obj, page_index_start + base + start, 1, static_cast<prot>(prot_val));
                } else {
                    while (index < lim_check) {
                        prot_temp = ((prots_[i] >> (index << 2))) & 0b1111;
                        index++;
                        
                        if (prot_temp == prot_val) {
                            count++;
                        }
                        
                        if ((prot_temp != prot_val) || (index == lim_check)) {
                            map->map(obj, page_index_start + base + start, count, static_cast<prot>(prot_val));
                            
                            count = 1;
                            start = index - 1;
                            prot_val = prot_temp;
                        }
                    }
                }
            }
        }
    }

    void page_array::supply_mapping(memory_object *obj, mapping *map) {
        for (std::size_t i = 0; i < total_segments_; i++) {
            if (segments_[i]) {
                segments_[i]->supply_mapping(obj, map, i, common::min<std::uint32_t>(pages_segment::TOTAL_PAGE_PER_SEGMENT,
                    static_cast<std::uint32_t>(total_pages_ - i * pages_segment::TOTAL_PAGE_PER_SEGMENT)));
            }
        }
    }
}