/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <catch2/catch.hpp>
#include <drivers/sensor/rotation.h>

TEST_CASE("Symbian rotation angles follow the device body axes", "[sensor]") {
    struct sample {
        double x, y, z;
        int rotation_x, rotation_y, rotation_z;
    };
    const sample cases[] = {
        { 0, 1, 0, 0, -1, 0 },
        { 0, -1, 0, 180, -1, 180 },
        { 1, 0, 0, -1, 90, 270 },
        { -1, 0, 0, -1, 270, 90 },
        { 0, 0, 1, 90, 180, -1 },
        { 0, 0, -1, 270, 0, -1 },
        { 1, 1, 1, 45, 135, 315 },
        { -1, -1, -1, 225, 315, 135 },
        { 1, -1, 1, 135, 135, 225 },
        { -1, 1, -1, 315, 315, 45 },
        { 0, 0, 0, -1, -1, -1 }
    };
    for (const auto &value : cases) {
        const auto data = eka2l1::drivers::rotation_from_acceleration(123456, value.x, value.y, value.z);
        CAPTURE(value.x, value.y, value.z);
        CHECK(data.timestamp_ == 123456);
        CHECK(data.x_ == value.rotation_x);
        CHECK(data.y_ == value.rotation_y);
        CHECK(data.z_ == value.rotation_z);
        CHECK(data.padding_ == 0);
    }
}

TEST_CASE("Symbian rotation wraps and quantizes independently of acceleration units", "[sensor]") {
    constexpr double radians = 3.14159265358979323846 / 180.0;
    for (int degrees = 0; degrees < 360; ++degrees) {
        const double x = -std::sin(degrees * radians);
        const double y = std::cos(degrees * radians);
        const auto data = eka2l1::drivers::rotation_from_acceleration(0, x, y, 0.5);
        const auto scaled = eka2l1::drivers::rotation_from_acceleration(0, x * 9.80665, y * 9.80665, 4.903325);
        CAPTURE(degrees);
        CHECK(data.z_ >= 0);
        CHECK(data.z_ < 360);
        CHECK(data.z_ % eka2l1::drivers::ROTATION_RESOLUTION_DEGREES == 0);
        CHECK(data.x_ == scaled.x_);
        CHECK(data.y_ == scaled.y_);
        CHECK(data.z_ == scaled.z_);
    }
}
