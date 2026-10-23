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
#include <services/notifier/queries.h>

#include <cstring>

namespace {
    std::vector<std::uint8_t> query_request() {
        // CAknGlobalMsgQuery::ShowMsgQueryL: signature, CBA resource, image IDs, tone, streamed text/header.
        const std::int32_t signature = 'a' * 'k' * 'n';
        std::vector<std::uint8_t> data = {
            0, 0, 0, 0, 0x24, 0x00, 0xCC, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0,
            3, 0, 12, 'Y', 'e', 's', 1, 0, 4, 0x0E, 0x65, 0xE5,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        std::memcpy(data.data(), &signature, sizeof(signature));
        return data;
    }

    std::vector<std::uint8_t> query_buttons() {
        // CBA and CBA_BUTTON from eikon.rh, including the resource reader's Unicode alignment byte.
        return {
            1, 0, 0, 0, 80, 0, 0, 0, 0, 0, 2, 0,
            0, 0xBD, 0x0B, 3, 'Y', 0, 'e', 0, 's', 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
            0, 0xBE, 0x0B, 2, 0xAB, 'N', 0, 'o', 0, 0, 0xFF, 0xFF, 0xFF, 0xFF
        };
    }
}

TEST_CASE("global_message_query_reads_streamed_unicode", "[notifier]") {
    auto data = query_request();
    eka2l1::epoc::notifier::message_query query;
    REQUIRE(eka2l1::epoc::notifier::read_message_query(data.data(), data.size(), query));
    REQUIRE(query.softkeys == 0x08CC0024);
    REQUIRE(query.text == u"Yes");
    REQUIRE(query.header == u"\u65E5");
}

TEST_CASE("global_message_query_rejects_bad_streams", "[notifier]") {
    const auto valid = query_request();
    eka2l1::epoc::notifier::message_query query;
    for (std::size_t size = 0; size < 26; ++size) {
        auto data = valid;
        REQUIRE_FALSE(eka2l1::epoc::notifier::read_message_query(data.data(), size, query));
    }
    for (const std::size_t offset : { 0, 15, 16, 21 }) {
        auto data = valid;
        data[offset] ^= 0x80;
        REQUIRE_FALSE(eka2l1::epoc::notifier::read_message_query(data.data(), data.size(), query));
    }
}

TEST_CASE("global_message_query_reads_long_text", "[notifier]") {
    auto data = query_request();
    data.erase(data.begin() + 14, data.begin() + 20);
    std::vector<std::uint8_t> text = { 100, 0, 0x21, 0x03 };
    text.insert(text.end(), 100, 'A');
    data.insert(data.begin() + 14, text.begin(), text.end());
    eka2l1::epoc::notifier::message_query query;
    REQUIRE(eka2l1::epoc::notifier::read_message_query(data.data(), data.size(), query));
    REQUIRE(query.text == std::u16string(100, u'A'));
    REQUIRE(query.header == u"\u65E5");
}

TEST_CASE("global_message_query_reads_four_byte_cardinality", "[notifier]") {
    auto data = query_request();
    data.erase(data.begin() + 14, data.begin() + 20);
    std::vector<std::uint8_t> text = { 0x00, 0x20, 0x03, 0x00, 0x02, 0x00 };
    text.insert(text.end(), 8192, 'A');
    data.insert(data.begin() + 14, text.begin(), text.end());
    eka2l1::epoc::notifier::message_query query;
    REQUIRE(eka2l1::epoc::notifier::read_message_query(data.data(), data.size(), query));
    REQUIRE(query.text == std::u16string(8192, u'A'));
    REQUIRE(query.header == u"\u65E5");
}

TEST_CASE("global_message_query_accepts_descriptor_width_and_capacity", "[notifier]") {
    auto data = query_request();
    data[14] = 4;
    data[16] = 14;
    data[19] = 0xE9;
    eka2l1::epoc::notifier::message_query query;
    REQUIRE(eka2l1::epoc::notifier::read_message_query(data.data(), data.size(), query));
    REQUIRE(query.text == u"Ye\u00E9");

    data.erase(data.begin() + 20, data.begin() + 26);
    data.insert(data.begin() + 20, { 0, 0, 0 });
    REQUIRE(eka2l1::epoc::notifier::read_message_query(data.data(), data.size(), query));
    REQUIRE(query.header.empty());
}

TEST_CASE("global_message_query_reads_rom_softkey_commands_and_labels", "[notifier]") {
    auto data = query_buttons();
    std::array<eka2l1::epoc::notifier::message_query_button, 2> buttons;
    REQUIRE(eka2l1::epoc::notifier::read_message_query_buttons(data.data(), data.size(), buttons));
    REQUIRE(buttons[0].command == 3005);
    REQUIRE(buttons[0].text == u"Yes");
    REQUIRE(buttons[1].command == 3006);
    REQUIRE(buttons[1].text == u"No");

    data[12] = 10;
    data.insert(data.begin() + 15, { 0, 0 });
    REQUIRE(eka2l1::epoc::notifier::read_message_query_buttons(data.data(), data.size(), buttons));
    REQUIRE(buttons[0].command == 3005);
    REQUIRE(buttons[1].text == u"No");
}

TEST_CASE("global_message_query_accepts_empty_softkeys", "[notifier]") {
    auto data = query_buttons();
    data.erase(data.begin() + 27, data.end());
    data.insert(data.end(), { 0, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF });
    std::array<eka2l1::epoc::notifier::message_query_button, 2> buttons;
    REQUIRE(eka2l1::epoc::notifier::read_message_query_buttons(data.data(), data.size(), buttons));
    REQUIRE(buttons[0].text == u"Yes");
    REQUIRE(buttons[1].text.empty());
}

TEST_CASE("global_message_query_rejects_invalid_softkey_resources", "[notifier]") {
    auto data = query_buttons();
    std::array<eka2l1::epoc::notifier::message_query_button, 2> buttons;
    for (std::size_t size = 0; size < data.size(); ++size) {
        REQUIRE_FALSE(eka2l1::epoc::notifier::read_message_query_buttons(data.data(), size, buttons));
    }
    data[10] = 1;
    REQUIRE_FALSE(eka2l1::epoc::notifier::read_message_query_buttons(data.data(), data.size(), buttons));
}
