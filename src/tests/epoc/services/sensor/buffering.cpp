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
#include <services/sensor/defs.h>

#include <limits>

TEST_CASE("Sensor buffering stays within the count advertised by OpenChannel", "[sensor]") {
    struct counts {
        std::uint32_t desired, maximum, expected_desired, expected_maximum;
    };
    const counts cases[] = {
        { 16, 16, 4, 4 },
        { 0, 0, 4, 4 },
        { 0, 2, 4, 4 },
        { 3, 2, 3, 4 },
        { 1, 4, 1, 4 },
        { 4, 1, 4, 4 },
        { 1, 0, 1, 4 },
        { std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max(), 4, 4 }
    };
    for (const auto &test : cases) {
        eka2l1::listening_parameters params{ test.desired, test.maximum, 50 };
        params.normalize_buffering_counts(eka2l1::SENSOR_MAX_BUFFERING_COUNT);
        CAPTURE(test.desired, test.maximum);
        REQUIRE(params.desired_buffering_count == test.expected_desired);
        REQUIRE(params.maximum_buffering_count == test.expected_maximum);
        REQUIRE(params.buffering_period == 50);
    }
}
