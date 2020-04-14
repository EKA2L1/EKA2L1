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

#include <epoc/services/akn/skin/bitmap_store.h>

#include <vector>

namespace eka2l1::epoc {

    void akn_skin_bitmap_store::store_bitmap(fbsbitmap *bitmap) {
        bitmap_vector.emplace_back(bitmap);
    }

    void akn_skin_bitmap_store::remove_stored_bitmap(const std::uint32_t bmp_handle) {
        if (!bmp_handle)
            return;

        auto bmp_to_remove = std::find_if(bitmap_vector.begin(), bitmap_vector.end(), [&](fbsbitmap *it) -> bool {
            return it->id == bmp_handle;
        });

        if (bmp_to_remove != bitmap_vector.end())
            bitmap_vector.erase(bmp_to_remove);
    }

    void akn_skin_bitmap_store::destroy_bitmaps() {
        bitmap_vector.clear();
        bitmap_vector.shrink_to_fit();
    }

} // namespace eka2l1
