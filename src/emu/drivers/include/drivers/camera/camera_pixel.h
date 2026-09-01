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

#include <drivers/camera/camera.h>

#include <cstdint>
#include <vector>

namespace eka2l1::drivers::camera {
    // Pixel contract shared by every backend that hands over a BGRA frame.

    // Formats such a backend can produce, mirroring the Android set.
    extern const frame_format SUPPORTED_FRAME_FORMATS[8];

    bool is_supported_frame_format(const frame_format format);

    // Repack a top-down BGRA buffer into the guest-facing layout, as Android
    // delivers it: FBS bitmap scanlines are 4-byte aligned, raw RGB565 rows are
    // tight, and ARGB8888 means R,G,B,A byte order.
    bool convert_bgra_to_guest(const std::uint8_t *src, const std::size_t src_stride,
        const int width, const int height, const frame_format format,
        std::vector<std::uint8_t> &dest);
}
