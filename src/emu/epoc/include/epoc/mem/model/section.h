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

#include <common/allocator.h>
#include <epoc/mem/common.h>

namespace eka2l1::mem {
    struct linear_section {
        vm_address beg_;
        vm_address end_;

        std::size_t psize_;

        common::bitmap_allocator alloc_;

        explicit linear_section(const vm_address start, const vm_address end, const std::size_t psize)
            : beg_(start)
            , end_(end)
            , psize_(psize)
            , alloc_((end - start) / psize) {
        }
    };
}
