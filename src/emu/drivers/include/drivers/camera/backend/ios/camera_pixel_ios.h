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

#include <CoreGraphics/CoreGraphics.h>

#include <drivers/camera/camera.h>
#include <drivers/camera/camera_pixel.h>

#include <cstdint>
#include <vector>

namespace eka2l1::drivers::camera {
    // CoreGraphics-side helpers of the iOS camera backend, shared with the
    // simulator one. The format set and the repacking they feed are in
    // camera_pixel.h.

    // Draw a CGImage into a top-down BGRX buffer of exactly dw x dh, rotated
    // counter-clockwise by rotation_ccw_deg (a multiple of 90) on the way.
    //
    // The destination is filled edge to edge, so a source whose rotated extents
    // have a different aspect ratio is stretched rather than letterboxed. That
    // is deliberate: a guest scales the frame over its own window, and the two
    // stretches largely cancel -- the way they do on the hardware, whose sensor
    // buffer has a fixed shape whatever the screen is.
    bool ios_render_cgimage_to_bgra(CGImageRef image, const int dw, const int dh,
        const int rotation_ccw_deg, std::vector<std::uint8_t> &out);

    // The angle, counter-clockwise, that a raw capture buffer must be rotated by
    // to be upright in the guest's picture. Built-in cameras output landscape
    // frames whatever the interface is doing, so a fixed offset takes them to
    // the host's natural orientation, and camera::frame_rotation() takes them
    // the rest of the way (see camera_collection.h).
    int ios_frame_rotation_ccw();

    // Wrap a top-down BGRX buffer as a CGImage. The buffer must outlive the image.
    CGImageRef ios_create_cgimage_from_bgra(const std::uint8_t *base, const std::size_t stride,
        const int width, const int height);

    // JPEG-encode a tightly packed top-down BGRA buffer.
    bool ios_encode_bgra_to_jpeg(const std::uint8_t *bgra, const int width, const int height,
        std::vector<std::uint8_t> &out);
}
