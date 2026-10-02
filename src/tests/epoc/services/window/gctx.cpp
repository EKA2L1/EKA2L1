/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <services/window/classes/gctx.h>
#include <services/fbs/fbs.h>

TEST_CASE("GC origin translates destinations without moving bitmap sources or established clips", "[window]") {
    using namespace eka2l1;
    epoc::graphic_context gc(nullptr, nullptr);
    gc.origin = { -200, -400 };
    epoc::gdi_store_command command;
    SECTION("bitmap destination and source use different coordinate spaces") {
        command.opcode_ = epoc::gdi_store_command_draw_bitmap;
        auto &data = command.get_data_struct<epoc::gdi_store_command_draw_bitmap_data>();
        data.dest_rect_ = rect({ 220, 440 }, { 30, 40 });
        data.source_rect_ = rect({ 1, 2 }, { 30, 40 });
        gc.apply_origin(command);
        REQUIRE(data.dest_rect_ == rect({ 20, 40 }, { 30, 40 }));
        REQUIRE(data.source_rect_ == rect({ 1, 2 }, { 30, 40 }));
    }
    SECTION("both line endpoints move") {
        command.opcode_ = epoc::gdi_store_command_draw_line;
        auto &data = command.get_data_struct<epoc::gdi_store_command_draw_line_data>();
        data.start_ = { 220, 440 };
        data.end_ = { 250, 480 };
        gc.apply_origin(command);
        REQUIRE(data.start_ == vec2(20, 40));
        REQUIRE(data.end_ == vec2(50, 80));
    }
    SECTION("polygon points move") {
        command.opcode_ = epoc::gdi_store_command_draw_polygon;
        auto &data = command.get_data_struct<epoc::gdi_store_command_draw_polygon_data>();
        point points[] = { { 220, 440 }, { 250, 480 }, { 220, 480 } };
        data.points_ = points;
        data.point_count_ = 3;
        gc.apply_origin(command);
        REQUIRE(points[0] == vec2(20, 40));
        REQUIRE(points[1] == vec2(50, 80));
        REQUIRE(points[2] == vec2(20, 80));
    }
    SECTION("clip rectangles already include the origin at SetClippingRect time") {
        command.opcode_ = epoc::gdi_store_command_set_clip_rect_single;
        auto &data = command.get_data_struct<epoc::gdi_store_command_set_clip_rect_single_data>();
        data.clipping_rect_ = rect({ 20, 40 }, { 30, 40 });
        gc.apply_origin(command);
        REQUIRE(data.clipping_rect_ == rect({ 20, 40 }, { 30, 40 }));
    }
    gc.reset_internal_status();
    REQUIRE(gc.origin == vec2(0, 0));
}

TEST_CASE("Pattern brush preserves tile phase across shapes and GC origins", "[window]") {
    using namespace eka2l1;
    epoc::bitwise_bitmap metadata{};
    metadata.header_.size_pixels = object_size(7, 5);
    fbsbitmap bitmap(nullptr, &metadata, false, false);
    bitmap.ref();

    epoc::graphic_context gc(nullptr, nullptr);
    gc.fill_mode = epoc::brush_style::pattern;
    gc.drawing_mode = epoc::draw_mode::exclusive_or;
    epoc::gdi_store_command command;
    REQUIRE_FALSE(gc.make_brush_fill_command(rect({ 0, 0 }, { 20, 10 }), command));

    gc.set_brush_pattern(&bitmap);
    REQUIRE(bitmap.count == 2);
    gc.set_brush_pattern(&bitmap);
    REQUIRE(bitmap.count == 2);
    gc.brush_origin = { 3, -2 };
    gc.origin = { 100, 200 };

    REQUIRE(gc.make_brush_fill_command(rect({ -1, -3 }, { 20, 10 }), command));
    REQUIRE(command.opcode_ == epoc::gdi_store_command_draw_bitmap);
    auto &data = command.get_data_struct<epoc::gdi_store_command_draw_bitmap_data>();
    REQUIRE(data.source_rect_ == rect({ 3, 4 }, { 20, 10 }));
    REQUIRE(data.gdi_flags_ == epoc::GDI_STORE_COMMAND_BRUSH_PATTERN);
    gc.apply_origin(command);
    REQUIRE(data.dest_rect_ == rect({ 99, 197 }, { 20, 10 }));
    REQUIRE(data.source_rect_.top == vec2(3, 4));

    REQUIRE(gc.make_brush_fill_command(rect({ 6, 2 }, { 20, 10 }), command));
    REQUIRE(command.get_data_struct<epoc::gdi_store_command_draw_bitmap_data>().source_rect_.top == vec2(3, 4));
    metadata.header_.size_pixels.x = 0;
    REQUIRE_FALSE(gc.make_brush_fill_command(rect({ 0, 0 }, { 20, 10 }), command));

    gc.reset_internal_status();
    REQUIRE(bitmap.count == 1);
    REQUIRE(gc.brush_pattern == nullptr);
    REQUIRE(gc.brush_origin == vec2(0, 0));
    REQUIRE(gc.drawing_mode == epoc::draw_mode::pen);
}
