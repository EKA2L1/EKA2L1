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
#include <services/window/protocol.h>
#include <services/window/framebuffer.h>

using namespace eka2l1;

TEST_CASE("Draw-device clears do not determine a raw framebuffer writer's depth", "[window][framebuffer]") {
    // Four pixels: a narrow RGB565 frame occupies the first eight bytes;
    // a wide BGRX frame also writes the following eight bytes.
    std::vector<std::uint8_t> pixels(16, 255);
    epoc::raw_framebuffer_depth_observer depth;
    depth.reset(pixels.data() + 8, 8);

    SECTION("a 32-bit startup clear followed by RGB565 stays narrow") {
        std::fill(pixels.begin(), pixels.end(), 0);
        depth.acknowledge_draw_device(pixels.data() + 8);
        const std::uint8_t rgb565[] = {0, 248, 224, 7, 31, 0, 255, 255};
        std::copy(std::begin(rgb565), std::end(rgb565), pixels.begin());
        REQUIRE_FALSE(depth.observe_raw(pixels.data() + 8));
        pixels[0] = 31;
        REQUIRE_FALSE(depth.observe_raw(pixels.data() + 8));
    }

    SECTION("a wide raw writer is detected after the startup clear") {
        std::fill(pixels.begin(), pixels.end(), 0);
        depth.acknowledge_draw_device(pixels.data() + 8);
        pixels[8] = 127;
        REQUIRE(depth.observe_raw(pixels.data() + 8));
        depth.acknowledge_draw_device(pixels.data() + 8);
        REQUIRE(depth.observe_raw(pixels.data() + 8));
        depth.reset(pixels.data() + 8, 8);
        REQUIRE_FALSE(depth.observe_raw(pixels.data() + 8));
    }

    SECTION("a raw-only client still widens when it writes past the narrow frame") {
        pixels[15] = 0;
        REQUIRE(depth.observe_raw(pixels.data() + 8));
    }
}

TEST_CASE("UIQ 2 wire opcodes preserve ROM export contracts", "[window]") {
    epoc::window_server_protocol uiq(epocver::epoc70, {1, 0, 151});
    REQUIRE(uiq.session_opcode(47) == ws_cl_op_claim_system_pointer_cursor_list);
    REQUIRE(uiq.session_opcode(78) == ws_cl_op_prepare_for_switch_off);
    REQUIRE(uiq.session_opcode(80) == ws_cl_op_log_command);
    REQUIRE(uiq.session_opcode(82) == ws_cl_op_send_event_to_one_window_group_per_cli);
    REQUIRE(uiq.window_opcode(12) == EWsWinOpSize);
    REQUIRE(uiq.window_opcode(13) == EWsWinOpActivate);
    REQUIRE(uiq.window_opcode(97) == EWsWinOpGetDisplayMode);
    REQUIRE(uiq.window_opcode(105) == EWsWinOpCaptureLongKey);
    REQUIRE(uiq.legacy_dsa());
}

TEST_CASE("Window protocol extensions do not shift other platform tables", "[window]") {
    epoc::window_server_protocol s60v1(epocver::epoc6, {1, 0, 139});
    REQUIRE(s60v1.session_opcode(80) == ws_cl_op_set_faded);
    epoc::window_server_protocol s60v2(epocver::epoc80, {1, 0, 151});
    REQUIRE(s60v2.session_opcode(ws_cl_op_log_command) == ws_cl_op_log_command);
    REQUIRE(s60v2.window_opcode(EWsWinOpAbsPosition) == EWsWinOpAbsPosition);
    REQUIRE(s60v2.legacy_dsa());
    epoc::window_server_protocol belle(epocver::epoc10, {1, 0, 151});
    REQUIRE(belle.window_opcode(EWsWinOpSendAdvancedPointerEvent) == EWsWinOpSendAdvancedPointerEvent);
    REQUIRE_FALSE(belle.legacy_dsa());
}

TEST_CASE("Series 90 keeps new session opcodes and old window opcodes", "[window]") {
    epoc::window_server_protocol s90(epocver::epoc7, {1, 0, 151});
    REQUIRE(s90.session_opcode(49) == ws_cl_op_claim_system_pointer_cursor_list);
    REQUIRE(s90.session_opcode(80) == ws_cl_op_prepare_for_switch_off);
    REQUIRE(s90.session_opcode(85) == ws_cl_op_send_event_to_one_window_group_per_cli);
    REQUIRE(s90.window_opcode(12) == EWsWinOpSize);
    REQUIRE(s90.window_opcode(24) == EWsWinOpInquireOffset);
    REQUIRE(s90.window_opcode(25) == EWsWinOpPointerFilter);
    REQUIRE(s90.window_opcode(61) == EWsWinOpSetName);
    REQUIRE(s90.window_opcode(94) == EWsWinOpEnableScreenChangeEvents);
    REQUIRE(s90.window_opcode(97) == EWsWinOpGetDisplayMode);
    REQUIRE(s90.window_opcode(105) == EWsWinOpEnableGroupListChangeEvents);
}

TEST_CASE("Mapped framebuffer tracks writes without a sentinel pixel value", "[window]") {
    epoc::framebuffer_observer observer;
    std::vector<std::uint8_t> pixels(16, 255);
    observer.reset(pixels.data(), 4, 4);
    REQUIRE_FALSE(observer.update(pixels.data(), 4, 4));
    pixels[0] = 0;
    pixels[12] = 0;
    REQUIRE(observer.update(pixels.data(), 4, 4));
    REQUIRE(observer.written_rows() == std::vector<bool>{true, false, false, true});
    pixels[0] = 255;
    REQUIRE(observer.update(pixels.data(), 4, 4));
    REQUIRE(observer.written_rows()[0]);
    REQUIRE_FALSE(observer.update(pixels.data(), 4, 4));
    pixels[4] = 12;
    observer.acknowledge(pixels.data());
    REQUIRE_FALSE(observer.update(pixels.data(), 4, 4));
    REQUIRE_FALSE(observer.written_rows()[1]);
    observer.reset(pixels.data(), 8, 2);
    REQUIRE(observer.written_rows() == std::vector<bool>{false, false});
    REQUIRE_FALSE(observer.update(pixels.data(), 8, 2));
}

TEST_CASE("Mapped panel updates scan the visible DSA region independently of dirty rectangles", "[window][framebuffer]") {
    const rect dirty_column(vec2(0, 0), vec2(1, 208)); // Symbian top-left and bottom-right.
    common::region visible;
    visible.add_rect(rect(vec2(0, 0), vec2(176, 208)));
    visible.eliminate(rect(vec2(40, 50), vec2(30, 20)));

    auto update = epoc::explicit_framebuffer_update_region(&dirty_column, 1, visible, true);
    REQUIRE(update.contains(vec2(100, 100)));
    REQUIRE(update.contains(vec2(0, 207)));
    REQUIRE_FALSE(update.contains(vec2(50, 60)));
    REQUIRE_FALSE(update.contains(vec2(177, 100)));

    common::region covered;
    REQUIRE(epoc::explicit_framebuffer_update_region(&dirty_column, 1, covered, true).empty());
    covered.add_rect(rect(vec2(80, 90), vec2(20, 30)));
    auto restarted = epoc::explicit_framebuffer_update_region(&dirty_column, 1, covered, true);
    REQUIRE(restarted.contains(vec2(85, 100)));
    REQUIRE_FALSE(restarted.contains(vec2(20, 100)));
}

TEST_CASE("Unmapped explicit updates retain dirty-rectangle clipping", "[window][framebuffer]") {
    const rect dirty(vec2(12, 20), vec2(30, 40)); // Symbian top-left and bottom-right.
    common::region visible;
    visible.add_rect(rect(vec2(0, 0), vec2(25, 100)));
    auto update = epoc::explicit_framebuffer_update_region(&dirty, 1, visible, false);
    REQUIRE(update.bounding_rect() == rect(vec2(12, 20), vec2(13, 20)));
    REQUIRE(update.contains(vec2(12, 20)));
    REQUIRE(update.contains(vec2(24, 39)));
    REQUIRE_FALSE(update.contains(vec2(26, 30)));
    REQUIRE_FALSE(update.contains(vec2(20, 41)));
    REQUIRE_FALSE(update.contains(vec2(10, 30)));
    REQUIRE(epoc::explicit_framebuffer_update_region(&dirty, 0, visible, false).empty());
}
