/*
 * Copyright (c) 2020 EKA2L1 Team
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

namespace eka2l1::kernel {
    class chunk;
}

namespace eka2l1 {
    using chunk_ptr = kernel::chunk*;
}

namespace eka2l1::epoc {
    class chunk_allocator : public common::block_allocator {
        chunk_ptr target_chunk;

    public:
        explicit chunk_allocator(chunk_ptr de_chunk);
        virtual bool expand(std::size_t target) override;
        
        template <typename T, typename ...Args>
        T *allocate_struct(Args... construct_args) {
            T *obj = reinterpret_cast<T *>(allocate(sizeof(T)));
            new (obj) T(construct_args...);

            return obj;
        }
    };
}