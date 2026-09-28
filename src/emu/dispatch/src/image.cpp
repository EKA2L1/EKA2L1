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

#include <dispatch/image.h>

#include <common/log.h>
#include <utils/err.h>

// Keep the implementation internal to this object: the Qt and Android
// frontends already carry their own copy of stb_image.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include <stb_image.h>

#include <cstring>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eimage_decode_info, const void *data, std::uint32_t size,
        image_decode_info *info) {
        if (!data || !info || (size == 0)) {
            return epoc::error_argument;
        }

        int width = 0;
        int height = 0;
        int components = 0;

        if (!stbi_info_from_memory(reinterpret_cast<const stbi_uc *>(data), static_cast<int>(size),
                &width, &height, &components)) {
            return epoc::error_not_supported;
        }

        info->width_ = static_cast<std::uint32_t>(width);
        info->height_ = static_cast<std::uint32_t>(height);
        info->components_ = static_cast<std::uint32_t>(components);

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eimage_decode, const void *data, std::uint32_t size,
        void *dest, std::uint32_t dest_size, std::uint32_t dest_stride, std::uint32_t dest_format) {
        if (!data || !dest || (size == 0) || (dest_size == 0)) {
            return epoc::error_argument;
        }

        std::uint32_t bytes_per_pixel = 4;
        switch (dest_format) {
        case image_decode_pixel_format_bgra8888:
        case image_decode_pixel_format_rgba8888:
            bytes_per_pixel = 4;
            break;

        case image_decode_pixel_format_rgb888:
            bytes_per_pixel = 3;
            break;

        default:
            return epoc::error_not_supported;
        }

        int width = 0;
        int height = 0;
        int components = 0;

        stbi_uc *pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(data),
            static_cast<int>(size), &width, &height, &components, 4);

        if (!pixels) {
            LOG_ERROR(HLE_DISPATCHER, "Unable to decode a {} byte image ({})", size, stbi_failure_reason());
            return epoc::error_corrupt;
        }

        const std::uint32_t row_bytes = static_cast<std::uint32_t>(width) * bytes_per_pixel;

        if (dest_stride < row_bytes) {
            stbi_image_free(pixels);
            return epoc::error_argument;
        }

        // The last row only needs its pixels, not the padding a stride implies,
        // so a buffer allocated as stride * (height - 1) + row_bytes is enough.
        const std::uint64_t needed = static_cast<std::uint64_t>(dest_stride) * (height - 1) + row_bytes;

        if (static_cast<std::uint64_t>(dest_size) < needed) {
            stbi_image_free(pixels);
            return epoc::error_overflow;
        }

        std::uint8_t *dest_bytes = reinterpret_cast<std::uint8_t *>(dest);

        for (int y = 0; y < height; y++) {
            const stbi_uc *src_row = pixels + static_cast<std::size_t>(y) * width * 4;
            std::uint8_t *dest_row = dest_bytes + static_cast<std::size_t>(y) * dest_stride;

            switch (dest_format) {
            case image_decode_pixel_format_rgba8888:
                std::memcpy(dest_row, src_row, row_bytes);
                break;

            case image_decode_pixel_format_bgra8888:
                for (int x = 0; x < width; x++) {
                    dest_row[x * 4 + 0] = src_row[x * 4 + 2];
                    dest_row[x * 4 + 1] = src_row[x * 4 + 1];
                    dest_row[x * 4 + 2] = src_row[x * 4 + 0];
                    dest_row[x * 4 + 3] = src_row[x * 4 + 3];
                }
                break;

            case image_decode_pixel_format_rgb888:
                for (int x = 0; x < width; x++) {
                    dest_row[x * 3 + 0] = src_row[x * 4 + 0];
                    dest_row[x * 3 + 1] = src_row[x * 4 + 1];
                    dest_row[x * 3 + 2] = src_row[x * 4 + 2];
                }
                break;

            default:
                break;
            }
        }

        stbi_image_free(pixels);
        return epoc::error_none;
    }
}
