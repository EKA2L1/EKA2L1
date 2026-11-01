// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace eka2l1::drivers {
    struct texture_upload_layout {
        std::size_t row_stride;
        std::size_t packed_size;
        std::size_t required_source_size;
    };

    inline bool get_texture_upload_layout(texture_upload_layout &layout, std::size_t width, std::size_t rows,
        std::size_t row_length, std::size_t bytes_per_pixel, std::uint32_t alignment) {
        const auto max = std::numeric_limits<std::size_t>::max();
        if (!width || !rows || !bytes_per_pixel || (alignment != 1 && alignment != 2 && alignment != 4 && alignment != 8)) {
            return false;
        }
        if (!row_length) {
            row_length = width;
        }
        if (width > max / bytes_per_pixel || row_length > (max - alignment + 1) / bytes_per_pixel) {
            return false;
        }
        const auto row_bytes = width * bytes_per_pixel;
        const auto stride = (row_length * bytes_per_pixel + alignment - 1) / alignment * alignment;
        if (rows > max / row_bytes || rows - 1 > (max - row_bytes) / stride) {
            return false;
        }
        layout = { stride, row_bytes * rows, (rows - 1) * stride + row_bytes };
        return true;
    }

    inline bool bgra8888_to_rgba(std::vector<std::uint8_t> &dest, const void *source, std::size_t source_size,
        std::size_t width, std::size_t rows, std::size_t row_length, std::uint32_t alignment) {
        texture_upload_layout layout;
        if (!source || !get_texture_upload_layout(layout, width, rows, row_length, 4, alignment)
            || source_size < layout.required_source_size) {
            return false;
        }
        dest.resize(layout.packed_size);
        const auto *bytes = static_cast<const std::uint8_t *>(source);
        for (std::size_t row = 0; row < rows; ++row) {
            for (std::size_t x = 0; x < width; ++x) {
                const auto *pixel = bytes + row * layout.row_stride + x * 4;
                auto *out = dest.data() + (row * width + x) * 4;
                out[0] = pixel[2];
                out[1] = pixel[1];
                out[2] = pixel[0];
                out[3] = pixel[3];
            }
        }
        return true;
    }

    // A zero source_size preserves the bitmap API's unspecified-length convention.
    inline bool rgb444_to_rgba4444(std::vector<std::uint16_t> &dest, const void *source, std::size_t source_size,
        std::size_t width, std::size_t rows, std::size_t row_length) {
        texture_upload_layout layout;
        if (!source || !get_texture_upload_layout(layout, width, rows, row_length, 2, 4)
            || (source_size && source_size < layout.required_source_size)) {
            return false;
        }
        const std::size_t output_row_length = (width + 1) & ~std::size_t(1);
        if (rows > std::numeric_limits<std::size_t>::max() / output_row_length / sizeof(std::uint16_t)) {
            return false;
        }
        dest.assign(output_row_length * rows, 0);
        const auto *bytes = static_cast<const std::uint8_t *>(source);
        for (std::size_t row = 0; row < rows; ++row) {
            for (std::size_t x = 0; x < width; ++x) {
                const auto *pixel = bytes + row * layout.row_stride + x * 2;
                const std::uint16_t value = pixel[0] | (static_cast<std::uint16_t>(pixel[1]) << 8);
                dest[row * output_row_length + x] = static_cast<std::uint16_t>((value << 4) | 0xF);
            }
        }
        return true;
    }
}
