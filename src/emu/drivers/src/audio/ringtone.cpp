/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <drivers/audio/ringtone.h>

#include <array>
#include <initializer_list>
#include <map>

// The ringing tone format is the one of the Nokia Smart Messaging Specification 3.0.0
// (section 3.6): a bit stream, most significant bit first.
namespace eka2l1::drivers {
    namespace {
        constexpr std::uint32_t COMMAND_RINGING_TONE_PROGRAMMING = 0x25;
        constexpr std::uint32_t COMMAND_UNICODE = 0x22;
        constexpr std::uint32_t COMMAND_SOUND = 0x1D;

        constexpr std::uint32_t SONG_TYPE_BASIC = 1;
        constexpr std::uint32_t SONG_TYPE_TEMPORARY = 2;

        constexpr std::uint32_t INSTRUCTION_NOTE = 1;
        constexpr std::uint32_t INSTRUCTION_SCALE = 2;
        constexpr std::uint32_t INSTRUCTION_STYLE = 3;
        constexpr std::uint32_t INSTRUCTION_TEMPO = 4;
        constexpr std::uint32_t INSTRUCTION_VOLUME = 5;

        constexpr std::uint32_t STYLE_CONTINUOUS = 1;
        constexpr std::uint32_t STYLE_STACCATO = 2;

        constexpr std::uint32_t LOOP_INFINITE = 15;

        constexpr std::array<std::uint32_t, 32> TEMPOS_BPM = { 25, 28, 31, 35, 40, 45, 50, 56, 63, 70, 80, 90, 100, 112,
            125, 140, 160, 180, 200, 225, 250, 285, 320, 355, 400, 450, 500, 565, 635, 715, 800, 900 };

        // A beat is a quarter note. 96 ticks make every length of the format whole: 1/32 2/3 is 8 ticks.
        constexpr std::uint32_t TICKS_PER_QUARTER = 96;
        constexpr std::array<std::uint32_t, 6> NOTE_TICKS = { 384, 192, 96, 48, 24, 12 };

        // The phone's buzzer sounds closest to a square wave.
        constexpr std::uint8_t PROGRAM_SQUARE_LEAD = 80;

        class bit_reader {
            const std::uint8_t *data_;
            std::size_t size_bits_;
            std::size_t pos_ = 0;

        public:
            explicit bit_reader(const std::uint8_t *data, const std::size_t size)
                : data_(data)
                , size_bits_(size * 8) {
            }

            bool read(const std::uint32_t count, std::uint32_t &value) {
                if (pos_ + count > size_bits_) {
                    return false;
                }

                value = 0;
                for (std::uint32_t i = 0; i < count; i++, pos_++) {
                    value = (value << 1) | ((data_[pos_ >> 3] >> (7 - (pos_ & 7))) & 1);
                }

                return true;
            }

            void align() {
                pos_ = (pos_ + 7) & ~static_cast<std::size_t>(7);
            }
        };

        struct instruction {
            std::uint32_t id;
            std::uint32_t value;
            std::uint32_t duration;
            std::uint32_t duration_specifier;
        };

        using pattern = std::vector<instruction>;

        class midi_writer {
            std::vector<std::uint8_t> track_;
            std::uint32_t pending_ticks_ = 0;
            std::uint32_t bpm_ = 0;

            void write_delta() {
                std::uint8_t bytes[4];
                int count = 0;
                std::uint32_t ticks = pending_ticks_;

                bytes[count++] = ticks & 0x7F;
                while (ticks >>= 7) {
                    bytes[count++] = 0x80 | (ticks & 0x7F);
                }

                while (count > 0) {
                    track_.push_back(bytes[--count]);
                }

                pending_ticks_ = 0;
            }

        public:
            void wait(const std::uint32_t ticks) {
                pending_ticks_ += ticks;
            }

            void event(std::initializer_list<std::uint8_t> bytes) {
                write_delta();
                track_.insert(track_.end(), bytes);
            }

            void tempo(const std::uint32_t bpm) {
                if (bpm == bpm_) {
                    return;
                }

                bpm_ = bpm;
                const std::uint32_t us_per_quarter = 60000000 / bpm;
                event({ 0xFF, 0x51, 0x03, static_cast<std::uint8_t>(us_per_quarter >> 16),
                    static_cast<std::uint8_t>(us_per_quarter >> 8), static_cast<std::uint8_t>(us_per_quarter) });
            }

            void finish(std::vector<std::uint8_t> &result) {
                event({ 0xFF, 0x2F, 0x00 });

                const std::uint32_t track_size = static_cast<std::uint32_t>(track_.size());
                result = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, TICKS_PER_QUARTER,
                    'M', 'T', 'r', 'k', static_cast<std::uint8_t>(track_size >> 24), static_cast<std::uint8_t>(track_size >> 16),
                    static_cast<std::uint8_t>(track_size >> 8), static_cast<std::uint8_t>(track_size) };
                result.insert(result.end(), track_.begin(), track_.end());
            }
        };

        // The settings an instruction changes stay for the rest of the song.
        struct player_state {
            std::uint32_t scale = 1;
            std::uint32_t style = 0;
            std::uint32_t volume = 7;
        };

        bool play_pattern(const pattern &instructions, player_state &state, midi_writer &writer) {
            for (const instruction &ins : instructions) {
                switch (ins.id) {
                case INSTRUCTION_NOTE: {
                    if (ins.duration >= NOTE_TICKS.size()) {
                        return false;
                    }

                    std::uint32_t length = NOTE_TICKS[ins.duration];
                    switch (ins.duration_specifier) {
                    case 1:
                        length = length * 3 / 2;
                        break;
                    case 2:
                        length = length * 7 / 4;
                        break;
                    case 3:
                        length = length * 2 / 3;
                        break;
                    default:
                        break;
                    }

                    // 0 is a pause, 1 to 12 are C to B; A of the first scale is 440 Hz.
                    if ((ins.value == 0) || (ins.value > 12) || (state.volume == 0)) {
                        writer.wait(length);
                        break;
                    }

                    std::uint32_t sounding = length;
                    if (state.style == STYLE_STACCATO) {
                        sounding = length / 2;
                    } else if (state.style != STYLE_CONTINUOUS) {
                        sounding = length - (length + 15) / 16;
                    }

                    const std::uint8_t key = static_cast<std::uint8_t>(59 + ins.value + 12 * state.scale);
                    const std::uint8_t velocity = static_cast<std::uint8_t>(state.volume * 127 / 15);

                    writer.event({ 0x90, key, velocity });
                    writer.wait(sounding);
                    writer.event({ 0x80, key, 0 });
                    writer.wait(length - sounding);
                    break;
                }

                case INSTRUCTION_SCALE:
                    state.scale = ins.value;
                    break;

                case INSTRUCTION_STYLE:
                    state.style = ins.value;
                    break;

                case INSTRUCTION_TEMPO:
                    writer.tempo(TEMPOS_BPM[ins.value]);
                    break;

                case INSTRUCTION_VOLUME:
                    state.volume = ins.value;
                    break;

                default:
                    return false;
                }
            }

            return true;
        }
    }

    bool is_nokia_ringtone(const std::uint8_t *data, const std::size_t size) {
        // The command part: its length in commands, then the ringing tone programming command
        return (size >= 3) && (data[0] >= 1) && (data[0] <= 4) && ((data[1] >> 1) == COMMAND_RINGING_TONE_PROGRAMMING);
    }

    bool nokia_ringtone_to_midi(const std::uint8_t *data, const std::size_t size, std::vector<std::uint8_t> &result) {
        if (!is_nokia_ringtone(data, size)) {
            return false;
        }

        bit_reader reader(data, size);
        std::uint32_t command_count = 0;
        std::uint32_t command = 0;
        bool unicode = false;

        if (!reader.read(8, command_count)) {
            return false;
        }

        for (std::uint32_t i = 0; i < command_count; i++) {
            if (!reader.read(7, command)) {
                return false;
            }

            if (command == COMMAND_SOUND) {
                break;
            }

            if ((command != COMMAND_RINGING_TONE_PROGRAMMING) && (command != COMMAND_UNICODE)) {
                return false;
            }

            unicode = unicode || (command == COMMAND_UNICODE);
            reader.align();
        }

        if (command != COMMAND_SOUND) {
            return false;
        }

        std::uint32_t song_type = 0;
        if (!reader.read(3, song_type)) {
            return false;
        }

        if (song_type == SONG_TYPE_BASIC) {
            std::uint32_t title_length = 0;
            std::uint32_t title_char = 0;

            if (!reader.read(4, title_length)) {
                return false;
            }

            for (std::uint32_t i = 0; i < title_length; i++) {
                if (!reader.read(unicode ? 16 : 8, title_char)) {
                    return false;
                }
            }
        } else if (song_type != SONG_TYPE_TEMPORARY) {
            return false;
        }

        std::uint32_t pattern_count = 0;
        if (!reader.read(8, pattern_count)) {
            return false;
        }

        midi_writer writer;
        player_state state;
        std::map<std::uint32_t, pattern> patterns;

        writer.tempo(TEMPOS_BPM[8]);
        writer.event({ 0xC0, PROGRAM_SQUARE_LEAD });

        for (std::uint32_t i = 0; i < pattern_count; i++) {
            std::uint32_t header_id = 0;
            std::uint32_t pattern_id = 0;
            std::uint32_t loop = 0;
            std::uint32_t instruction_count = 0;

            if (!reader.read(3, header_id) || !reader.read(2, pattern_id) || !reader.read(4, loop) || !reader.read(8, instruction_count)) {
                return false;
            }

            if (header_id != 0) {
                return false;
            }

            // A pattern of no instruction plays again the pattern of that id defined before.
            if (instruction_count != 0) {
                pattern &instructions = patterns[pattern_id];
                instructions.clear();

                for (std::uint32_t j = 0; j < instruction_count; j++) {
                    instruction ins{};
                    bool ok = reader.read(3, ins.id);

                    switch (ins.id) {
                    case INSTRUCTION_NOTE:
                        ok = ok && reader.read(4, ins.value) && reader.read(3, ins.duration) && reader.read(2, ins.duration_specifier);
                        break;

                    case INSTRUCTION_SCALE:
                    case INSTRUCTION_STYLE:
                        ok = ok && reader.read(2, ins.value);
                        break;

                    case INSTRUCTION_TEMPO:
                        ok = ok && reader.read(5, ins.value);
                        break;

                    case INSTRUCTION_VOLUME:
                        ok = ok && reader.read(4, ins.value);
                        break;

                    default:
                        ok = false;
                        break;
                    }

                    if (!ok) {
                        return false;
                    }

                    instructions.push_back(ins);
                }
            }

            auto defined = patterns.find(pattern_id);
            if (defined == patterns.end()) {
                continue;
            }

            // The player repeats the whole sound when the game asks; an endless pattern plays once here.
            const std::uint32_t times = (loop == LOOP_INFINITE) ? 1 : (loop + 1);
            for (std::uint32_t t = 0; t < times; t++) {
                if (!play_pattern(defined->second, state, writer)) {
                    return false;
                }
            }
        }

        writer.finish(result);
        return true;
    }
}
