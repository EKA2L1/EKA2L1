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
#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>
#include <services/window/util.h>
#include <vector>

using namespace eka2l1;

// econs fills every 4x7 character cell with its own rectangle. At a non-integer
// display scale the scaled cells must still tile: each cell's right/bottom edge is
// the next cell's left/top edge, otherwise the window background shows through as
// a grid of 1px lines.
TEST_CASE("scale_rectangle_keeps_adjacent_cells_watertight", "[window]") {
    const float scales[] = { 2.0833333f, 1.5f, 1.3333334f, 2.7f, 3.0f };

    for (const float scale : scales) {
        for (int x = 0; x < 240; x += 4) {
            eka2l1::rect left(eka2l1::vec2(x, 0), eka2l1::vec2(4, 7));
            eka2l1::rect right(eka2l1::vec2(x + 4, 7), eka2l1::vec2(4, 7));

            scale_rectangle(left, scale);
            scale_rectangle(right, scale);

            INFO("scale " << scale << " cell x " << x);
            REQUIRE(left.top.x + left.size.x == right.top.x);
            REQUIRE(left.top.y + left.size.y == right.top.y);
        }
    }
}

TEST_CASE("scale_rectangle_integer_scale_is_exact", "[window]") {
    eka2l1::rect r(eka2l1::vec2(12, 7), eka2l1::vec2(4, 7));
    scale_rectangle(r, 2.0f);

    REQUIRE(r.top == eka2l1::vec2(24, 14));
    REQUIRE(r.size == eka2l1::vec2(8, 14));
}

TEST_CASE("Adjacent redraw clips cover the same pixels as the scaled drawing", "[window]") {
    for (const float scale : { 1.5f, 2.0089433f, 2.7f }) {
        drivers::graphics_command_builder builder;
        for (const rect area : { rect({ 88, 289 }, { 41, 10 }), rect({ 129, 289 }, { 54, 10 }) }) {
            common::region region;
            region.add_rect(area);
            builder.clip_bitmap_region(region, scale);
        }

        auto commands = builder.retrieve_command_list();
        std::vector<rect> clips;
        for (std::size_t i = 0; i < commands.size_; ++i) {
            const auto &command = commands.base_[i];
            if (command.opcode_ == drivers::graphics_driver_clip_bitmap_rect) {
                rect clip;
                drivers::unpack_u64_to_2u32(command.data_[0], clip.top.x, clip.top.y);
                drivers::unpack_u64_to_2u32(command.data_[1], clip.size.x, clip.size.y);
                clips.push_back(clip);
            }
        }
        delete[] commands.base_;

        REQUIRE(clips.size() == 2);
        REQUIRE(clips[0].bottom_right().x == clips[1].top.x);
        REQUIRE(clips[0].top == vec2(88, 289) * scale);
        REQUIRE(clips[1].bottom_right() == vec2(183, 299) * scale);
    }
}
