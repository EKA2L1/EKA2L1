/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <drivers/graphics/common.h>
#include <drivers/itc.h>
#include <epoc/services/fbs/bitmap.h>

#include <array>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::epoc {
    constexpr std::uint32_t MAX_CACHE_SIZE = 1024;

    class bitmap_cache {
    public:
        using driver_texture_handle_array = std::array<drivers::handle, MAX_CACHE_SIZE>;
        using bitmap_array = std::array<epoc::bitwise_bitmap*, MAX_CACHE_SIZE>;
        using timestamps_array = std::array<std::uint64_t, MAX_CACHE_SIZE>;
        using hashes_array = timestamps_array;

    private:
        driver_texture_handle_array driver_textures;
        bitmap_array                bitmaps;
        timestamps_array            timestamps;
        hashes_array                hashes;

        std::uint8_t *base_large_chunk;

        kernel_system *kern;
        std::weak_ptr<drivers::graphics_driver_client> cli;

        std::int64_t last_free { 0 };

    protected:
        std::uint64_t hash_bitwise_bitmap(epoc::bitwise_bitmap *bw_bmp);

    public:
        explicit bitmap_cache(kernel_system *kern_);

        std::int64_t get_suitable_bitmap_index();

        /**
         * \brief   Add a bitmap to texture cache if not available in the cache, and get
         *          the driver's texture handle.
         * 
         * If the cache is full, this will find the least used bitmap (by sorting out 
         * last used timestamp). Also, since bitwise bitmap modify itself by user's will
         * without a method to notify the user, this also hashes bitmap data (using xxHash),
         * and will reupload the bitmap to driver if the texture data is different.
         * 
         * \param   bmp The pointer to bitwise bitmap.
         * \returns Handle to driver's texture associated with this bitmap.
         */
        drivers::handle add_or_get(epoc::bitwise_bitmap *bmp);

        /**
         * \brief   Remove the bitmap from cache.
         * \returns True if success. False if bitmap not found. Likely that the bitmap has been
         *          purged from cache
         */
        bool remove(epoc::bitwise_bitmap *bmp);
    };
}
