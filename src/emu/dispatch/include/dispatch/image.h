/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <dispatch/def.h>

#include <cstdint>

namespace eka2l1::dispatch {
    // Pixel layouts the guest can ask the decoded image to be written in. The
    // names describe the byte order in memory, low address first, which is what
    // a guest image buffer is indexed with.
    enum image_decode_pixel_format {
        image_decode_pixel_format_bgra8888 = 0,     // Qt's Format_RGB32 / Format_ARGB32 on a little-endian guest
        image_decode_pixel_format_rgba8888 = 1,
        image_decode_pixel_format_rgb888 = 2
    };

    struct image_decode_info {
        std::uint32_t width_;
        std::uint32_t height_;
        std::uint32_t components_;      // 1 = grey, 3 = RGB, 4 = RGB + alpha
    };

    /**
     * @brief Read the dimensions of an encoded image without decoding it.
     *
     * Cheap enough to call before allocating the destination buffer: only the
     * container header is parsed.
     *
     * @param data  Encoded image bytes.
     * @param size  Length of the encoded image in bytes.
     * @param info  Receives the image's dimensions and component count.
     */
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eimage_decode_info, const void *data, std::uint32_t size,
        image_decode_info *info);

    /**
     * @brief Decode an encoded image into a guest buffer.
     *
     * The destination is written row by row using dest_stride, so a guest image
     * whose rows are padded (every toolkit pads to some alignment) can be filled
     * without a second copy.
     *
     * @param data          Encoded image bytes.
     * @param size          Length of the encoded image in bytes.
     * @param dest          Destination buffer, at least dest_stride * height bytes.
     * @param dest_size     Size of the destination buffer, in bytes.
     * @param dest_stride   Distance between two destination rows, in bytes.
     * @param dest_format   One of image_decode_pixel_format.
     */
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eimage_decode, const void *data, std::uint32_t size,
        void *dest, std::uint32_t dest_size, std::uint32_t dest_stride, std::uint32_t dest_format);
}
