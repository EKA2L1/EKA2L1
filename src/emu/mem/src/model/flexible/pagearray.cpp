/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <mem/model/flexible/pagearray.h>
#include <common/log.h>
#include <common/algorithm.h>

namespace eka2l1::mem::flexible {    
    static constexpr std::uint64_t FULL_QWORD_MASK = 0xFFFFFFFFFFFFFFFF;

    pages_segment::pages_segment() {
        std::memset(prots_, 0, sizeof(prots_));
    }

    void pages_segment::alter(const std::uint32_t index, const std::uint32_t count, const prot perm, const bool is_clear) {
        if ((index + count > TOTAL_PAGE_PER_SEGMENT) || (count == 0)) {
            return;
        }

        std::uint32_t current_index = index;
        std::uint32_t end_index = index + count;
        std::uint32_t qword_index = index >> PAGE_PER_QWORD_SHIFT;
        std::uint64_t perm_u64 = static_cast<std::uint64_t>(perm) & 0b1111;

        const std::uint32_t index_in_first_qword = index & PAGE_INDEX_MASK_IN_QWORD;
        const std::uint32_t count_first_qword = common::min<std::uint32_t>(count, 16 - index_in_first_qword);
        
        if (index_in_first_qword != 0) {
            std::uint64_t mask_set = ((~(FULL_QWORD_MASK >> (count_first_qword << 2))) >> (index_in_first_qword << 2));
            prots_[qword_index] &= ~mask_set;

            if (!is_clear) {
                for (std::uint32_t i = 0; i < count_first_qword; i++) {
                    prots_[qword_index] |= (perm_u64 << ((i + index_in_first_qword) << 2));
                }
            }
            
            qword_index++;
            current_index += count_first_qword;
        }

        if (current_index >= end_index) {
            return;
        }

        std::uint32_t count_this_iter = 0;
        std::uint64_t full_mask_built = 0;

        while (current_index < end_index) {
            count_this_iter = common::min<std::uint32_t>(end_index - current_index, PAGE_PER_QWORD);
            if (count_this_iter != PAGE_PER_QWORD) {
                std::uint64_t mask_set = (~(FULL_QWORD_MASK >> (count_this_iter << 2)));
                prots_[qword_index] &= ~mask_set;

                if (!is_clear) {
                    for (std::uint32_t i = 0; i < count_this_iter; i++) {
                        prots_[qword_index] |= (perm_u64 << (i << 2));
                    }
                }
            } else {
                if (is_clear) {
                    prots_[qword_index] = 0;
                } else {
                    if (!full_mask_built) {
                        for (std::uint32_t i = 0; i < PAGE_PER_QWORD; i++) {
                            full_mask_built |= (perm_u64 << (i << 2));
                        }
                    }

                    prots_[qword_index] = full_mask_built;
                }
            }

            qword_index++;
            current_index += count_this_iter;
        }
    }

    page_array::page_array(const std::uint32_t total_pages)
        : segments_(nullptr)
        , total_pages_(total_pages)
        , total_segments_(0) {
        total_segments_ = (total_pages + pages_segment::TOTAL_PAGE_PER_SEGMENT - 1) / pages_segment::TOTAL_PAGE_PER_SEGMENT;
        segments_ = new pages_segment*[total_segments_];
        std::memset(segments_, 0, sizeof(pages_segment*) * total_segments_);
    }

    page_array::~page_array() {
        for (std::uint32_t i = 0; i < total_segments_; i++) {
            if (segments_[i]) {
                delete segments_[i];
            }
        }

        delete segments_;
    }

    void page_array::alter(const std::uint32_t index, const std::uint32_t count, const prot perm, const bool is_clear) {
        if (index + count > total_pages_) {
            return;
        }

        std::uint32_t offset_in_segment = (index & (pages_segment::TOTAL_PAGE_PER_SEGMENT - 1));
        std::uint32_t segment_index = index / pages_segment::TOTAL_PAGE_PER_SEGMENT;
        std::uint32_t index_end = (index + count);
        std::uint32_t index_current = index;
        std::uint32_t page_elim = 0;

        if (offset_in_segment != 0) {
            page_elim = common::min<std::uint32_t>(pages_segment::TOTAL_PAGE_PER_SEGMENT - offset_in_segment, count);
            if (!segments_[segment_index]) {
                segments_[segment_index] = new pages_segment;
            }

            segments_[segment_index]->alter(offset_in_segment, page_elim, perm, is_clear);

            index_current += page_elim;
            segment_index++;
        }

        while (index_current < index_end) {
            page_elim = common::min<std::uint32_t>(pages_segment::TOTAL_PAGE_PER_SEGMENT, index_end - index_current);
            if (!segments_[segment_index]) {
                segments_[segment_index] = new pages_segment;
            }

            segments_[segment_index]->alter(0, page_elim, perm, is_clear);
            index_current += page_elim;
            segment_index++;
        }
    }
}