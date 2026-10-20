// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace eka2l1::dispatch {
    constexpr std::uint64_t TONE_DURATION_FOREVER = ~0ULL;

    // KMdaRepeatForever
    constexpr std::int32_t TONE_REPEAT_FOREVER = -2;

    struct tone_instruction {
        enum kind_type : std::uint8_t {
            TONE,
            LOOP_START,
            LOOP_END,
            RESTART
        };

        kind_type kind = TONE;

        // Both frequencies zero is silence.
        std::uint32_t frequency_one = 0;
        std::uint32_t frequency_two = 0;
        std::uint64_t duration_us = 0;

        // Total passes through a loop; zero loops until stopped.
        std::uint32_t loop_count = 0;
    };

    using tone_program = std::vector<tone_instruction>;

    tone_program make_tone_program(const std::uint32_t frequency_one, const std::uint32_t frequency_two,
        const std::uint64_t duration_us);

    // The Nokia tone sequence bytecode carried by Avkon sound resources (AvkonSystemSounds.ra).
    bool parse_nokia_tone_sequence(const std::uint8_t *data, const std::size_t size, tone_program &program);

    // A Nokia smart messaging ringing tone, the .rng/OTA format written by Composer.
    bool parse_smart_messaging_ringtone(const std::uint8_t *data, const std::size_t size, tone_program &program);

    // A negative tone-on length plays the first digit until stopped (KMdaInfiniteDurationDTMFToneOnLength).
    bool make_dtmf_tone_program(const std::u16string &digits, const std::int64_t tone_on_us,
        const std::uint64_t tone_off_us, const std::uint64_t pause_us, tone_program &program);

    // Every pass, including the last, is followed by the trailing silence, as DevSound plays it.
    std::optional<std::uint64_t> tone_program_duration_us(const tone_program &program, const std::int32_t repeats,
        const std::uint64_t trailing_silence_us);

    class tone_generator {
        tone_program program_;
        std::uint32_t sample_rate_;
        std::int32_t repeats_left_;
        std::uint64_t trailing_silence_samples_;

        std::size_t pc_ = 0;
        std::vector<std::pair<std::size_t, std::uint32_t>> loops_;
        bool trailing_silence_played_ = false;

        std::uint32_t frequency_one_ = 0;
        std::uint32_t frequency_two_ = 0;
        std::uint64_t tone_samples_ = 0;
        std::uint64_t tone_position_ = 0;
        double phase_one_ = 0.0;
        double phase_two_ = 0.0;

        bool next_tone();

    public:
        explicit tone_generator(tone_program program, const std::uint32_t sample_rate, const std::int32_t repeats,
            const std::uint64_t trailing_silence_us);

        // Mono samples; returns fewer than requested once the program has ended.
        std::size_t generate(std::int16_t *samples, const std::size_t count);
    };
}
