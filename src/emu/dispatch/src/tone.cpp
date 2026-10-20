// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <dispatch/tone.h>

#include <algorithm>
#include <cmath>

namespace eka2l1::dispatch {
    namespace {
        enum nokia_tone_opcode : std::uint8_t {
            NOKIA_OPCODE_SEQUENCE_START = 0,
            NOKIA_OPCODE_CONTINUE_MASK = 2,
            NOKIA_OPCODE_REPEAT = 5,
            NOKIA_OPCODE_REPEAT_END = 6,
            NOKIA_OPCODE_RESTART = 7,
            NOKIA_OPCODE_TONE_INDICATION = 10,
            NOKIA_OPCODE_END = 11,
            NOKIA_OPCODE_DURATION_UNIT = 17,
            NOKIA_OPCODE_TONE_FIRST = 64,
            NOKIA_OPCODE_TONE_LAST = 127
        };

        constexpr std::uint32_t NOKIA_DEFAULT_DURATION_UNIT_MS = 10;
        constexpr std::uint8_t NOKIA_TONE_TICK_FOREVER = 0;

        constexpr double PI = 3.14159265358979323846;
        constexpr double TONE_AMPLITUDE = 16384.0;
        constexpr std::uint32_t TONE_RAMP_MS = 4;

        // Bounds a program whose loops never reach a tone with a length.
        constexpr std::size_t TONE_MAX_STEPS_WITHOUT_SAMPLES = 1 << 16;

        // AUD_TONE_NETWORK_FREQ_1..4 sit at indices 14-17; the note indices around them are an
        // equal-tempered scale with index 34 at 440 Hz, which matches every frequency Avkon documents.
        std::uint32_t nokia_tone_frequency(const std::uint32_t index) {
            static constexpr std::uint32_t NETWORK_FREQUENCIES[] = { 425, 950, 1400, 1800 };

            if (index == 0) {
                return 0;
            }

            if ((index >= 14) && (index <= 17)) {
                return NETWORK_FREQUENCIES[index - 14];
            }

            return static_cast<std::uint32_t>(std::lround(440.0 * std::pow(2.0, (static_cast<double>(index) - 34.0) / 12.0)));
        }

        tone_instruction make_tone(const std::uint32_t frequency_one, const std::uint32_t frequency_two,
            const std::uint64_t duration_us) {
            tone_instruction instruction;
            instruction.frequency_one = frequency_one;
            instruction.frequency_two = frequency_two;
            instruction.duration_us = duration_us;

            return instruction;
        }

        bool get_dtmf_frequencies(const char16_t digit, std::uint32_t &low, std::uint32_t &high) {
            static constexpr std::uint32_t LOW[] = { 697, 770, 852, 941 };
            static constexpr std::uint32_t HIGH[] = { 1209, 1336, 1477, 1633 };

            // Row and column of each key on the 4x4 DTMF keypad.
            static constexpr char KEYPAD[] = "123A456B789C*0#D";

            char16_t key = digit;
            if ((key >= u'a') && (key <= u'f')) {
                key = key - u'a' + u'A';
            }

            if (key == u'E') {
                key = u'*';
            } else if (key == u'F') {
                key = u'#';
            }

            for (std::size_t i = 0; i < sizeof(KEYPAD) - 1; i++) {
                if (static_cast<char16_t>(KEYPAD[i]) == key) {
                    low = LOW[i / 4];
                    high = HIGH[i % 4];

                    return true;
                }
            }

            return false;
        }

        bool is_dtmf_space(const char16_t c) {
            return (c == u' ') || (c == u'\t') || (c == u'\n') || (c == u'\r') || (c == u'\f') || (c == u'\v');
        }
    }

    tone_program make_tone_program(const std::uint32_t frequency_one, const std::uint32_t frequency_two,
        const std::uint64_t duration_us) {
        return { make_tone(frequency_one, frequency_two, duration_us) };
    }

    bool parse_nokia_tone_sequence(const std::uint8_t *data, const std::size_t size, tone_program &program) {
        if (!data || (size == 0) || (data[0] != NOKIA_OPCODE_SEQUENCE_START)) {
            return false;
        }

        tone_program parsed;
        std::uint32_t unit_ms = NOKIA_DEFAULT_DURATION_UNIT_MS;

        for (std::size_t i = 1; i < size;) {
            const std::uint8_t opcode = data[i++];
            const bool has_argument = (i < size);

            if ((opcode >= NOKIA_OPCODE_TONE_FIRST) && (opcode <= NOKIA_OPCODE_TONE_LAST)) {
                if (!has_argument) {
                    return false;
                }

                const std::uint8_t tick = data[i++];
                const std::uint64_t duration_us = (tick == NOKIA_TONE_TICK_FOREVER) ? TONE_DURATION_FOREVER
                                                                                     : static_cast<std::uint64_t>(tick) * unit_ms * 1000;

                parsed.push_back(make_tone(nokia_tone_frequency(opcode - NOKIA_OPCODE_TONE_FIRST), 0, duration_us));
                continue;
            }

            switch (opcode) {
            case NOKIA_OPCODE_END:
                program = std::move(parsed);
                return true;

            case NOKIA_OPCODE_DURATION_UNIT:
                if (!has_argument) {
                    return false;
                }

                unit_ms = data[i++];
                break;

            case NOKIA_OPCODE_CONTINUE_MASK:
            case NOKIA_OPCODE_TONE_INDICATION:
                if (!has_argument) {
                    return false;
                }

                i++;
                break;

            case NOKIA_OPCODE_REPEAT: {
                if (!has_argument) {
                    return false;
                }

                tone_instruction instruction;
                instruction.kind = tone_instruction::LOOP_START;
                instruction.loop_count = data[i++];

                parsed.push_back(instruction);
                break;
            }

            case NOKIA_OPCODE_REPEAT_END: {
                tone_instruction instruction;
                instruction.kind = tone_instruction::LOOP_END;

                parsed.push_back(instruction);
                break;
            }

            case NOKIA_OPCODE_RESTART: {
                tone_instruction instruction;
                instruction.kind = tone_instruction::RESTART;

                parsed.push_back(instruction);
                break;
            }

            default:
                return false;
            }
        }

        program = std::move(parsed);
        return true;
    }

    bool parse_smart_messaging_ringtone(const std::uint8_t *data, const std::size_t size, tone_program &program) {
        if (!data || (size == 0)) {
            return false;
        }

        std::size_t bit = 0;
        const auto read = [&](const std::uint32_t count, std::uint32_t &value) {
            if (bit + count > size * 8) {
                return false;
            }

            value = 0;
            for (std::uint32_t i = 0; i < count; i++, bit++) {
                value = (value << 1) | ((data[bit / 8] >> (7 - bit % 8)) & 1);
            }

            return true;
        };

        static constexpr std::uint32_t BEATS_PER_MINUTE[] = { 25, 28, 31, 35, 40, 45, 50, 56, 63, 70, 80, 90, 100, 112,
            125, 140, 160, 180, 200, 225, 250, 285, 320, 355, 400, 450, 500, 565, 635, 715, 800, 900 };

        // Defaults from the smart messaging specification: scale-2 (A = 880 Hz), natural style, 160 bpm.
        std::uint32_t scale = 1;
        std::uint32_t style = 0;
        std::uint32_t tempo = 16;

        const auto read_pattern = [&](const std::uint32_t count, tone_program &pattern) {
            for (std::uint32_t i = 0; i < count; i++) {
                std::uint32_t id = 0;
                std::uint32_t value = 0;

                if (!read(3, id)) {
                    return false;
                }

                switch (id) {
                case 1: {
                    std::uint32_t length = 0;
                    std::uint32_t specifier = 0;

                    if (!read(4, value) || !read(3, length) || !read(2, specifier)) {
                        return false;
                    }

                    // A whole note is four beats; dotted, double dotted and triplet scale it by 3/2, 7/4 and 2/3.
                    static constexpr std::uint32_t SPECIFIER_NUMERATOR[] = { 1, 3, 7, 2 };
                    static constexpr std::uint32_t SPECIFIER_DENOMINATOR[] = { 1, 2, 4, 3 };

                    const std::uint64_t duration_us = 240000000ULL * SPECIFIER_NUMERATOR[specifier]
                        / (BEATS_PER_MINUTE[tempo] * (1ULL << std::min<std::uint32_t>(length, 5)) * SPECIFIER_DENOMINATOR[specifier]);

                    std::uint32_t frequency = 0;
                    if ((value >= 1) && (value <= 12)) {
                        frequency = static_cast<std::uint32_t>(std::lround(440.0 * (1 << scale)
                            * std::pow(2.0, (static_cast<double>(value) - 10.0) / 12.0)));
                    }

                    // Gap lengths for natural and staccato are approximations; legato plays the note through.
                    const std::uint64_t sounding_us = (style == 1) ? duration_us : ((style == 2) ? duration_us / 2 : duration_us * 7 / 8);

                    pattern.push_back(make_tone(frequency, 0, sounding_us));
                    if (sounding_us != duration_us) {
                        pattern.push_back(make_tone(0, 0, duration_us - sounding_us));
                    }

                    break;
                }

                case 2:
                    if (!read(2, scale)) {
                        return false;
                    }

                    break;

                case 3:
                    if (!read(2, style)) {
                        return false;
                    }

                    break;

                case 4:
                    if (!read(5, tempo)) {
                        return false;
                    }

                    break;

                case 5:
                    if (!read(4, value)) {
                        return false;
                    }

                    break;

                default:
                    return false;
                }
            }

            return true;
        };

        const auto read_song = [&](tone_program &song) {
            tone_program patterns[4];
            std::uint32_t pattern_count = 0;

            if (!read(8, pattern_count)) {
                return false;
            }

            for (std::uint32_t i = 0; i < pattern_count; i++) {
                std::uint32_t header = 0;
                std::uint32_t id = 0;
                std::uint32_t loop = 0;
                std::uint32_t instruction_count = 0;

                if (!read(3, header) || (header != 0) || !read(2, id) || !read(4, loop) || !read(8, instruction_count)) {
                    return false;
                }

                // An instruction count of zero replays the pattern defined earlier under the same id.
                if (instruction_count != 0) {
                    patterns[id].clear();
                    if (!read_pattern(instruction_count, patterns[id])) {
                        return false;
                    }
                }

                if (loop != 0) {
                    tone_instruction start;
                    start.kind = tone_instruction::LOOP_START;
                    start.loop_count = (loop == 15) ? 0 : loop + 1;

                    song.push_back(start);
                }

                song.insert(song.end(), patterns[id].begin(), patterns[id].end());

                if (loop != 0) {
                    tone_instruction end;
                    end.kind = tone_instruction::LOOP_END;

                    song.push_back(end);
                }
            }

            return true;
        };

        bool programming = false;
        bool unicode = false;
        bool has_song = false;
        tone_program parsed;

        std::uint32_t command_count = 0;
        while (read(8, command_count) && (command_count != 0)) {
            for (std::uint32_t i = 0; i < command_count; i++) {
                std::uint32_t command = 0;
                if (!read(7, command)) {
                    return false;
                }

                if (command == 0x25) {
                    programming = true;
                } else if (command == 0x22) {
                    unicode = true;
                } else if (command == 0x05) {
                    std::uint32_t cancelled = 0;
                    if (!read(7, cancelled)) {
                        return false;
                    }

                    if (cancelled == 0x22) {
                        unicode = false;
                    }
                } else if (command == 0x1D) {
                    std::uint32_t song_type = 0;
                    if (!programming || !read(3, song_type)) {
                        return false;
                    }

                    // Basic songs carry a title, temporary ones do not.
                    if (song_type == 1) {
                        std::uint32_t title_length = 0;
                        if (!read(4, title_length) || (bit + title_length * (unicode ? 16 : 8) > size * 8)) {
                            return false;
                        }

                        bit += title_length * (unicode ? 16 : 8);
                    }

                    if ((song_type == 1) || (song_type == 2)) {
                        if (!read_song(parsed)) {
                            return false;
                        }

                        has_song = true;
                    }
                }

                // Every command part is padded with filler bits to the next byte.
                bit = (bit + 7) & ~std::size_t(7);
            }
        }

        if (!has_song) {
            return false;
        }

        program = std::move(parsed);
        return true;
    }

    bool make_dtmf_tone_program(const std::u16string &digits, const std::int64_t tone_on_us,
        const std::uint64_t tone_off_us, const std::uint64_t pause_us, tone_program &program) {
        tone_program built;

        for (const char16_t digit : digits) {
            if (is_dtmf_space(digit)) {
                continue;
            }

            if (digit == u',') {
                built.push_back(make_tone(0, 0, pause_us));
                continue;
            }

            std::uint32_t low = 0;
            std::uint32_t high = 0;

            if (!get_dtmf_frequencies(digit, low, high)) {
                return false;
            }

            if (tone_on_us < 0) {
                program = make_tone_program(low, high, TONE_DURATION_FOREVER);
                return true;
            }

            built.push_back(make_tone(low, high, static_cast<std::uint64_t>(tone_on_us)));
            built.push_back(make_tone(0, 0, tone_off_us));
        }

        program = std::move(built);
        return true;
    }

    std::optional<std::uint64_t> tone_program_duration_us(const tone_program &program, const std::int32_t repeats,
        const std::uint64_t trailing_silence_us) {
        // (duration before the loop, passes) for each open loop.
        std::vector<std::pair<std::uint64_t, std::uint32_t>> loops;
        std::uint64_t duration = 0;

        for (const tone_instruction &instruction : program) {
            switch (instruction.kind) {
            case tone_instruction::TONE:
                if (instruction.duration_us == TONE_DURATION_FOREVER) {
                    return std::nullopt;
                }

                duration += instruction.duration_us;
                break;

            case tone_instruction::LOOP_START:
                loops.emplace_back(duration, instruction.loop_count);
                duration = 0;
                break;

            case tone_instruction::LOOP_END: {
                if (loops.empty()) {
                    break;
                }

                const auto [before, passes] = loops.back();
                loops.pop_back();

                if (passes == 0) {
                    if (duration != 0) {
                        return std::nullopt;
                    }
                } else {
                    duration *= passes;
                }

                duration += before;
                break;
            }

            case tone_instruction::RESTART:
                for (const auto &loop : loops) {
                    duration += loop.first;
                }

                if (duration != 0) {
                    return std::nullopt;
                }

                loops.clear();
                break;
            }
        }

        // A loop left open simply runs once to the end of the program.
        for (const auto &loop : loops) {
            duration += loop.first;
        }

        const std::uint64_t pass = duration + trailing_silence_us;
        if (pass == 0) {
            return 0;
        }

        if (repeats == TONE_REPEAT_FOREVER) {
            return std::nullopt;
        }

        return pass * (static_cast<std::uint64_t>(std::max<std::int32_t>(repeats, 0)) + 1);
    }

    tone_generator::tone_generator(tone_program program, const std::uint32_t sample_rate, const std::int32_t repeats,
        const std::uint64_t trailing_silence_us)
        : program_(std::move(program))
        , sample_rate_(sample_rate)
        , repeats_left_((repeats == TONE_REPEAT_FOREVER) ? TONE_REPEAT_FOREVER : std::max<std::int32_t>(repeats, 0))
        , trailing_silence_samples_(trailing_silence_us * sample_rate / 1000000) {
    }

    bool tone_generator::next_tone() {
        for (std::size_t steps = 0; steps < TONE_MAX_STEPS_WITHOUT_SAMPLES; steps++) {
            if (pc_ >= program_.size()) {
                if (!trailing_silence_played_ && trailing_silence_samples_) {
                    trailing_silence_played_ = true;

                    frequency_one_ = 0;
                    frequency_two_ = 0;
                    tone_samples_ = trailing_silence_samples_;
                    tone_position_ = 0;

                    return true;
                }

                if (repeats_left_ == 0) {
                    return false;
                }

                if (repeats_left_ > 0) {
                    repeats_left_--;
                }

                pc_ = 0;
                loops_.clear();
                trailing_silence_played_ = false;

                continue;
            }

            const tone_instruction &instruction = program_[pc_++];

            switch (instruction.kind) {
            case tone_instruction::TONE: {
                const std::uint64_t samples = (instruction.duration_us == TONE_DURATION_FOREVER)
                    ? ~0ULL
                    : instruction.duration_us * sample_rate_ / 1000000;

                if (samples == 0) {
                    break;
                }

                frequency_one_ = instruction.frequency_one;
                frequency_two_ = instruction.frequency_two;
                tone_samples_ = samples;
                tone_position_ = 0;
                phase_one_ = 0.0;
                phase_two_ = 0.0;

                return true;
            }

            case tone_instruction::LOOP_START:
                loops_.emplace_back(pc_, instruction.loop_count);
                break;

            case tone_instruction::LOOP_END: {
                if (loops_.empty()) {
                    break;
                }

                auto &loop = loops_.back();

                if ((loop.second == 0) || (--loop.second != 0)) {
                    pc_ = loop.first;
                } else {
                    loops_.pop_back();
                }

                break;
            }

            case tone_instruction::RESTART:
                pc_ = 0;
                loops_.clear();
                break;
            }
        }

        return false;
    }

    std::size_t tone_generator::generate(std::int16_t *samples, const std::size_t count) {
        const std::uint64_t ramp_samples = std::max<std::uint64_t>(1, sample_rate_ * TONE_RAMP_MS / 1000);
        std::size_t produced = 0;

        while (produced < count) {
            if (tone_position_ >= tone_samples_) {
                if (!next_tone()) {
                    break;
                }
            }

            const std::uint64_t left = tone_samples_ - tone_position_;
            const std::size_t run = static_cast<std::size_t>(std::min<std::uint64_t>(count - produced, left));

            if ((frequency_one_ == 0) && (frequency_two_ == 0)) {
                std::fill(samples + produced, samples + produced + run, 0);
            } else {
                const double step_one = 2.0 * PI * frequency_one_ / sample_rate_;
                const double step_two = 2.0 * PI * frequency_two_ / sample_rate_;
                const bool dual = (frequency_one_ != 0) && (frequency_two_ != 0);

                for (std::size_t i = 0; i < run; i++) {
                    const std::uint64_t position = tone_position_ + i;

                    // Short ramps at both ends keep the steps between tones from clicking.
                    double gain = std::min(1.0, static_cast<double>(position) / ramp_samples);
                    if (tone_samples_ != ~0ULL) {
                        gain = std::min(gain, static_cast<double>(tone_samples_ - position) / ramp_samples);
                    }

                    double value = std::sin(phase_one_) + std::sin(phase_two_);
                    if (dual) {
                        value *= 0.5;
                    }

                    samples[produced + i] = static_cast<std::int16_t>(value * gain * TONE_AMPLITUDE);

                    phase_one_ = std::fmod(phase_one_ + step_one, 2.0 * PI);
                    phase_two_ = std::fmod(phase_two_ + step_two, 2.0 * PI);
                }
            }

            tone_position_ += run;
            produced += run;
        }

        return produced;
    }
}
