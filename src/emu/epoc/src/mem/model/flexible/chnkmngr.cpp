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

#include <epoc/mem/model/flexible/chnkmngr.h>

namespace eka2l1::mem::flexible {
    chunk_manager::chunk_manager() {
    }

    flexible_mem_model_chunk *chunk_manager::new_chunk(mmu_base *mmu, const asid id) {
        for (std::size_t i = 0; i < chunks_.size(); i++) {
            if (!chunks_[i]) {
                chunks_[i] = std::make_unique<flexible_mem_model_chunk>(mmu, id);
                return chunks_[i].get();
            }
        }

        // Push new chunk into it
        chunks_.push_back(std::make_unique<flexible_mem_model_chunk>(mmu, id));
        return chunks_.back().get();
    }

    bool chunk_manager::destroy(flexible_mem_model_chunk *chunk) {
        // Search if this chunk available in the manager
        auto ite = std::find_if(chunks_.begin(), chunks_.end(), [chunk](std::unique_ptr<flexible_mem_model_chunk> &instance) {
            return instance.get() == chunk;
        });

        if (ite == chunks_.end()) {
            // Noooo, where is your mom, did you get lost? Get lost
            return false;
        }

        chunks_.erase(ite);
        return true;
    }
}