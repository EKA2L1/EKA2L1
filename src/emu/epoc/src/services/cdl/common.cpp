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

#include <epoc/services/cdl/common.h>
#include <common/chunkyseri.h>

#include <algorithm>
#include <vector>

namespace eka2l1::epoc {
    void do_refs_state(common::chunkyseri &seri, cdl_ref_collection &collection_) {
        std::vector<std::u16string> names;

        for (std::size_t i = 0; i < collection_.size(); i++) {
            if (!std::binary_search(names.begin(), names.end(), collection_[i].name_)) {
                names.push_back(collection_[i].name_);
                std::sort(names.begin(), names.end());
            }
        }

        // Now serialize
        seri.absorb_container(names, [](common::chunkyseri &seri, std::u16string &str) {
            std::uint32_t dat_size = static_cast<std::uint32_t>(str.length()) * 2;
            seri.absorb(dat_size);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                str.resize(dat_size / 2);
            }

            seri.absorb_impl(reinterpret_cast<std::uint8_t*>(&str[0]), dat_size);
        });

        seri.absorb_container(collection_, [&](common::chunkyseri &seri, cdl_ref &ref) {
            seri.absorb(ref.id_);
            seri.absorb(ref.uid_);

            std::uint32_t name_idx = static_cast<std::uint32_t>(std::distance(names.begin(),
                std::lower_bound(names.begin(), names.end(), ref.name_)));

            seri.absorb(name_idx);

            if (seri.get_seri_mode() == common::SERI_MODE_READ) {
                ref.name_ = names[name_idx];
            }
        });
    }
}
