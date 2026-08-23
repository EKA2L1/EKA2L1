/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <catch2/catch.hpp>
#include <drivers/audio/stream.h>

TEST_CASE("Audio frame positions use frames rather than channel samples", "[audio]") {
    using eka2l1::drivers::frames_to_microseconds;

    REQUIRE(frames_to_microseconds(48000, 48000) == 1000000);
    REQUIRE(frames_to_microseconds(22050, 44100) == 500000);
    REQUIRE(frames_to_microseconds(1234, 0) == 0);
}
