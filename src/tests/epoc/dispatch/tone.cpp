// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

// Expected values come from the opcode comments in Avkon's AvkonSystemSounds.ra and
// the phone engine's defaultbeep.rss.

#include <catch2/catch.hpp>
#include <dispatch/tone.h>

#include <cstdint>
#include <vector>

using namespace eka2l1::dispatch;

TEST_CASE("nokia_tone_voice_recording_start_tone", "[tone]") {
    const std::uint8_t sequence[] = { 0, 17, 10, 103, 6, 105, 6, 11 };
    tone_program program;

    REQUIRE(parse_nokia_tone_sequence(sequence, sizeof(sequence), program));
    REQUIRE(program.size() == 2);
    REQUIRE(program[0].frequency_one == 587);
    REQUIRE(program[0].duration_us == 60000);
    REQUIRE(program[1].frequency_one == 659);
    REQUIRE(tone_program_duration_us(program, 0, 0) == 120000);
}

TEST_CASE("nokia_tone_repeat_counts_total_passes", "[tone]") {
    // r_net_radio_not_available: 425 Hz on 200 ms, off 200 ms, 3 bursts.
    const std::uint8_t sequence[] = { 0, 17, 10, 5, 3, 78, 20, 64, 20, 6, 11 };
    tone_program program;

    REQUIRE(parse_nokia_tone_sequence(sequence, sizeof(sequence), program));
    REQUIRE(program[1].frequency_one == 425);
    REQUIRE(tone_program_duration_us(program, 0, 0) == 1200000);
}

TEST_CASE("nokia_tone_forever_and_restart_never_end", "[tone]") {
    const std::uint8_t forever[] = { 0, 17, 10, 2, 252, 10, 128, 78, 0, 11 };
    const std::uint8_t restart[] = { 0, 17, 10, 2, 252, 78, 100, 5, 4, 64, 100, 6, 7, 11 };
    tone_program program;

    REQUIRE(parse_nokia_tone_sequence(forever, sizeof(forever), program));
    REQUIRE(!tone_program_duration_us(program, 0, 0).has_value());

    REQUIRE(parse_nokia_tone_sequence(restart, sizeof(restart), program));
    REQUIRE(!tone_program_duration_us(program, 0, 0).has_value());
}

TEST_CASE("nokia_tone_rejects_other_formats", "[tone]") {
    const std::uint8_t sqnc[] = { 'S', 'Q', 'N', 'C', 0, 0 };
    tone_program program;

    REQUIRE(!parse_nokia_tone_sequence(sqnc, sizeof(sqnc), program));
}

TEST_CASE("tone_repeats_add_trailing_silence_to_every_pass", "[tone]") {
    const tone_program program = make_tone_program(440, 0, 100000);

    REQUIRE(tone_program_duration_us(program, 2, 50000) == 450000);
    REQUIRE(!tone_program_duration_us(program, TONE_REPEAT_FOREVER, 0).has_value());
}

TEST_CASE("tone_generator_produces_the_program_length", "[tone]") {
    tone_generator generator(make_tone_program(1000, 0, 10000), 8000, 1, 5000);
    std::vector<std::int16_t> samples(1000);

    // Two passes of 80 tone samples and 40 silent ones.
    REQUIRE(generator.generate(samples.data(), samples.size()) == 240);
}

TEST_CASE("dtmf_digits_map_to_keypad_frequencies", "[tone]") {
    tone_program program;

    REQUIRE(make_dtmf_tone_program(u"1, #", 250000, 50000, 250000, program));
    REQUIRE(program.size() == 5);
    REQUIRE(program[0].frequency_one == 697);
    REQUIRE(program[0].frequency_two == 1209);
    REQUIRE(program[2].duration_us == 250000);
    REQUIRE(program[3].frequency_one == 941);
    REQUIRE(program[3].frequency_two == 1477);
    REQUIRE(!make_dtmf_tone_program(u"1x", 250000, 50000, 250000, program));
}
