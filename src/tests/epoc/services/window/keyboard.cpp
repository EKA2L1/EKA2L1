/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <services/window/keyboard.h>

using namespace eka2l1::epoc;

static event raw_key(std_scan_code scan, bool pressed) {
    event result{};
    result.type = pressed ? event_code::key_down : event_code::key_up;
    result.key_evt_ = {0, scan, 0, 0};
    return result;
}

// CKeyTranslatorX::TranslateKey suppresses modifier keypresses; WSERV queues
// key up/down after updating its stored modifier state (ky_tran.cpp, EVENT.CPP).
TEST_CASE("Edit key transitions carry Shift without generating character or repeat events", "[window][keyboard]") {
    key_event_translator translator;
    auto down = raw_key(std_key_left_shift, true);
    REQUIRE_FALSE(translator.translate(down));
    REQUIRE(down.key_evt_.code == 0);
    REQUIRE(down.key_evt_.modifiers == (event_modifier_left_shift | event_modifier_shift));
    REQUIRE_FALSE(translator.is_repeatable(std_key_left_shift));

    auto up = raw_key(std_key_left_shift, false);
    REQUIRE_FALSE(translator.translate(up));
    REQUIRE(up.key_evt_.modifiers == 0);
    REQUIRE(translator.modifiers() == 0);
}

TEST_CASE("Edit key modifies navigation and softkeys until released", "[window][keyboard]") {
    key_event_translator translator;
    auto edit = raw_key(std_key_left_shift, true);
    translator.translate(edit);

    auto right = raw_key(std_key_right_arrow, true);
    auto key = translator.translate(right);
    REQUIRE(key);
    REQUIRE(key->type == event_code::key);
    REQUIRE(key->key_evt_.code == key_right_arrow);
    REQUIRE(key->key_evt_.modifiers == (event_modifier_shift | event_modifier_left_shift | event_modifier_repeatable));
    REQUIRE(right.key_evt_.modifiers == (event_modifier_shift | event_modifier_left_shift));

    auto soft = raw_key(std_key_device_0, true);
    auto soft_key = translator.translate(soft);
    REQUIRE(soft_key);
    REQUIRE(soft_key->key_evt_.modifiers == (event_modifier_shift | event_modifier_left_shift));

    edit.type = event_code::key_up;
    translator.translate(edit);
    // Repeats sample the current stored state, not the original keydown's flags.
    REQUIRE(translator.modifiers() == 0);
    right.type = event_code::key_up;
    REQUIRE_FALSE(translator.translate(right));
    REQUIRE(right.key_evt_.modifiers == 0);
    right.type = event_code::key_down;
    REQUIRE(translator.translate(right)->key_evt_.modifiers == event_modifier_repeatable);
}

TEST_CASE("Releasing either Shift preserves the other held Shift", "[window][keyboard]") {
    key_event_translator translator;
    auto left = raw_key(std_key_left_shift, true);
    auto right = raw_key(std_key_right_shift, true);
    translator.translate(left);
    translator.translate(right);
    REQUIRE(translator.modifiers() == (event_modifier_left_shift | event_modifier_right_shift | event_modifier_shift));
    translator.translate(left);
    left.type = event_code::key_up;
    translator.translate(left);
    REQUIRE(left.key_evt_.modifiers == (event_modifier_right_shift | event_modifier_shift));
    REQUIRE_FALSE(translator.is_repeatable(std_key_right_shift));
    right.type = event_code::key_up;
    translator.translate(right);
    REQUIRE(right.key_evt_.modifiers == 0);
}
