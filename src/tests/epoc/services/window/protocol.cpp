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
