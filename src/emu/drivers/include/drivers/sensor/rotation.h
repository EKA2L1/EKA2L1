/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace eka2l1::drivers {
    constexpr int ROTATION_RESOLUTION_DEGREES = 15;

#pragma pack(push, 1)
    struct sensor_rotation_data {
        std::uint64_t timestamp_;
        std::int32_t x_;
        std::int32_t y_;
        std::int32_t z_;
        std::int32_t padding_ = 0;
    };
#pragma pack(pop)

    static_assert(sizeof(sensor_rotation_data) == 24);

    inline sensor_rotation_data rotation_from_acceleration(const std::uint64_t timestamp,
        const double x, const double y, const double z) {
        auto angle = [](const double sine, const double cosine) -> std::int32_t {
            // Rotation around an axis parallel to gravity is undefined.
            if (std::hypot(sine, cosine) < 1e-6) {
                return -1;
            }
            constexpr double radians_to_degrees = 180.0 / 3.14159265358979323846;
            double degrees = std::atan2(sine, cosine) * radians_to_degrees;
            if (degrees < 0) {
                degrees += 360.0;
            }
            return ((static_cast<int>(degrees) + ROTATION_RESOLUTION_DEGREES / 2)
                / ROTATION_RESOLUTION_DEGREES * ROTATION_RESOLUTION_DEGREES) % 360;
        };

        // Symbian Orientation SSY uses Y as zero for X/Z, and -Z as zero for Y.
        return { timestamp, angle(z, y), angle(x, -z), angle(-x, y), 0 };
    }
}
