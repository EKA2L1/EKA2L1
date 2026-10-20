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
#include <services/hwrm/haptics/protocol.h>
#include <utils/err.h>

using namespace eka2l1::epoc;
using namespace eka2l1::epoc::haptics;

namespace {
    std::vector<std::uint8_t> bytes(const std::string &hex) {
        std::vector<std::uint8_t> result;
        for (std::size_t i = 0; i < hex.size(); i += 2) {
            result.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        }
        return result;
    }

    const auto open_device = bytes("120002000000");
    // X7 Vibe packetizer: MagSweep(100 ms, magnitude 5000, smooth, no envelope).
    const auto sweep = bytes("1c00010000006400000088130000000000000000000000000000000000000000000000");
    const auto periodic = bytes("1d00010000006400000088130000011400000000000000000000000000000000000000");
    const auto state = bytes("1b000100000001000000");
    const auto pause_effect = bytes("19000100000001000000");
    const auto resume_effect = bytes("1a000100000001000000");
    const auto stop_effect = bytes("07000100000001000000");

    void expect_integer(const response &response, const std::int32_t value) {
        REQUIRE(response.error == error_none);
        REQUIRE(response.data.size() == 4);
        const auto &b = response.data;
        const auto result = b[0] | (b[1] << 8) | (b[2] << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
        CHECK(static_cast<std::int32_t>(result) == value);
    }
}

TEST_CASE("Vibe device open and capability replies match the ROM and SDK", "[haptics]") {
    device dev;
    CHECK(dev.execute(sweep, 0).error == error_bad_handle);
    expect_integer(dev.execute(open_device, 0), 1);
    CHECK(dev.execute(open_device, 0).error == error_already_exists);
    expect_integer(dev.execute(bytes("0900010200000002000000"), 0), 1);
    expect_integer(dev.execute(bytes("0900010200000009000000"), 0), 3);
    CHECK(dev.execute(bytes("090001020000000e000000"), 0).error == error_not_supported);
    CHECK(dev.execute(bytes("130001000000"), 0).error == error_none);
    CHECK_FALSE(dev.opened());
    CHECK(dev.execute(sweep, 0).error == error_bad_handle);
}

TEST_CASE("Vibe effects expire and pause time does not consume duration", "[haptics]") {
    for (const auto &packet : {sweep, periodic}) {
        device dev;
        expect_integer(dev.execute(open_device, 0), 1);
        expect_integer(dev.execute(packet, 1000), 1);
        CHECK(dev.output(1000) == vibration{100, 50});
        expect_integer(dev.execute(state, 1020), 1);
        REQUIRE(dev.execute(pause_effect, 1020).error == error_none);
        expect_integer(dev.execute(state, 5000), 2);
        CHECK(dev.output(5000).duration == 0);
        REQUIRE(dev.execute(resume_effect, 5000).error == error_none);
        CHECK(dev.output(5000) == vibration{80, 50});
        expect_integer(dev.execute(state, 5079), 1);
        expect_integer(dev.execute(state, 5080), 0);
        CHECK(dev.output(5080).duration == 0);
    }
}

TEST_CASE("Modifying a Vibe effect keeps its elapsed time", "[haptics]") {
    device dev;
    dev.execute(open_device, 0);
    expect_integer(dev.execute(sweep, 0), 1);
    // ModifyMagSweep(handle 1, duration 60 ms, magnitude 10000).
    const auto modify = bytes("1e0001000000010000003c00000010270000000000000000000000000000000000000000000000");
    REQUIRE(dev.execute(modify, 40).error == error_none);
    CHECK(dev.output(40) == vibration{20, 100});
    expect_integer(dev.execute(state, 60), 0);
}

TEST_CASE("Vibe slot replacement preserves paused effects until an explicit stop", "[haptics]") {
    device dev;
    dev.execute(open_device, 0);
    expect_integer(dev.execute(sweep, 0), 1);
    REQUIRE(dev.execute(pause_effect, 20).error == error_none);
    expect_integer(dev.execute(periodic, 30), 2);
    expect_integer(dev.execute(state, 30), 2);
    REQUIRE(dev.execute(bytes("19000200000001000000"), 40).error == error_none);
    expect_integer(dev.execute(sweep, 50), 3);

    REQUIRE(dev.execute(resume_effect, 60).error == error_none);
    expect_integer(dev.execute(state, 60), 1);
    expect_integer(dev.execute(bytes("1b000200000001000000"), 60), 2);
    expect_integer(dev.execute(bytes("1b000300000001000000"), 60), 0);
    CHECK(dev.output(60) == vibration{80, 50});
    expect_integer(dev.execute(state, 140), 0);

    REQUIRE(dev.execute(bytes("1a000200000001000000"), 150).error == error_none);
    CHECK(dev.output(150) == vibration{90, 50});
    REQUIRE(dev.execute(bytes("19000200000001000000"), 160).error == error_none);
    expect_integer(dev.execute(sweep, 170), 4);
    REQUIRE(dev.execute(bytes("080001000000"), 180).error == error_none);
    expect_integer(dev.execute(bytes("1b000200000001000000"), 180), 0);
    expect_integer(dev.execute(bytes("1b000400000001000000"), 180), 0);
    CHECK(dev.output(180).duration == 0);
}

TEST_CASE("Vibe stop, disable and strength have per-device scope", "[haptics]") {
    device dev, other;
    dev.execute(open_device, 0);
    other.execute(open_device, 0);
    dev.execute(sweep, 0);
    other.execute(sweep, 0);
    REQUIRE(dev.execute(stop_effect, 5).error == error_none);
    expect_integer(dev.execute(state, 5), 0);
    expect_integer(other.execute(state, 5), 1);

    // SetDeviceProperty(Strength, 5000), then play a new effect.
    REQUIRE(dev.execute(bytes("10000100000003000000010488130000"), 10).error == error_none);
    expect_integer(dev.execute(sweep, 10), 2);
    CHECK(dev.output(10) == vibration{100, 25});
    REQUIRE(dev.execute(bytes("10000100000002000000020101"), 11).error == error_none);
    expect_integer(dev.execute(bytes("1100020100000002000000"), 11), 1);
    CHECK(dev.output(11).duration == 0);
    expect_integer(dev.execute(sweep, 12), 3);
    CHECK(dev.output(12).duration == 0);
    REQUIRE(dev.execute(bytes("10000100000002000000020100"), 13).error == error_none);
    CHECK(dev.output(13).duration == 0);
    expect_integer(dev.execute(sweep, 14), 4);
    CHECK(dev.output(14) == vibration{100, 25});
    dev.reset();
    CHECK_FALSE(dev.opened());
    CHECK(other.output(14) == vibration{86, 50});
}

TEST_CASE("Vibe packet validation rejects truncation and does not start an effect", "[haptics]") {
    for (const auto &packet : {sweep, periodic, state, pause_effect, stop_effect,
        bytes("0900010200000002000000"), bytes("10000100000003000000010488130000")}) {
        for (std::size_t size = 0; size < packet.size(); ++size) {
            device dev;
            dev.execute(open_device, 0);
            const auto response = dev.execute(std::vector<std::uint8_t>(packet.begin(), packet.begin() + size), 0);
            CAPTURE(size, packet.size());
            CHECK(response.error != error_none);
            CHECK(dev.output(0).duration == 0);
        }
    }
    device dev;
    dev.execute(open_device, 0);
    auto bad = sweep;
    bad[10] = 0xff;
    bad[11] = 0xff;
    CHECK(dev.execute(bad, 0).error == error_argument);
    CHECK(dev.execute(bytes("ffff01000000"), 0).error == error_not_supported);
    expect_integer(dev.execute(sweep, 0), 1);
    auto foreign_device = state;
    foreign_device[6] = 2;
    CHECK(dev.execute(foreign_device, 0).error == error_bad_handle);
    expect_integer(dev.execute(state, 1), 1);
}

TEST_CASE("Infinite Vibe duration survives long runtimes and old stopped handles stay queryable", "[haptics]") {
    device dev;
    dev.execute(open_device, 0);
    auto infinite = sweep;
    infinite[6] = 0xff;
    infinite[7] = 0xff;
    infinite[8] = 0xff;
    infinite[9] = 0x7f;
    expect_integer(dev.execute(infinite, 0), 1);
    expect_integer(dev.execute(state, 6000000000ULL), 1);
    CHECK(dev.output(6000000000ULL) == vibration{0x7fffffff, 50});
    REQUIRE(dev.execute(stop_effect, 6000000001ULL).error == error_none);
    expect_integer(dev.execute(sweep, 6000000002ULL), 2);
    expect_integer(dev.execute(state, 6000000002ULL), 0);
    CHECK(dev.output(6000000002ULL) == vibration{100, 50});
}
