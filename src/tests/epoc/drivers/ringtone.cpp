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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <drivers/audio/ringtone.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {
    class bit_writer {
        std::vector<std::uint8_t> data_;
        std::size_t bits_ = 0;

    public:
        void put(const std::uint32_t value, const std::uint32_t count) {
            for (std::uint32_t i = count; i > 0; i--, bits_++) {
                if ((bits_ & 7) == 0) {
                    data_.push_back(0);
                }

                data_.back() |= ((value >> (i - 1)) & 1) << (7 - (bits_ & 7));
            }
        }

        const std::vector<std::uint8_t> &data() const {
            return data_;
        }
    };

    // A temporary song: pattern A (tempo 125, loudest volume, one quarter A) played twice,
    // then played once more through an already-defined pattern header.
    std::vector<std::uint8_t> make_ringtone() {
        bit_writer writer;

        writer.put(2, 8);    // two commands
        writer.put(0x25, 7); // ringing tone programming
        writer.put(0, 1);    // filler
        writer.put(0x1D, 7); // sound
        writer.put(2, 3);    // temporary song type
        writer.put(2, 8);    // two patterns

        writer.put(0, 3); // pattern header
        writer.put(0, 2); // pattern A
        writer.put(1, 4); // one loop
        writer.put(3, 8); // three instructions
        writer.put(4, 3); // tempo
        writer.put(14, 5); // 125 bpm
        writer.put(5, 3); // volume
        writer.put(15, 4); // level 15
        writer.put(1, 3); // note
        writer.put(10, 4); // A
        writer.put(2, 3); // quarter
        writer.put(0, 2); // no duration specifier

        writer.put(0, 3); // pattern header
        writer.put(0, 2); // pattern A
        writer.put(0, 4); // no loop
        writer.put(0, 8); // already defined

        return writer.data();
    }

    struct midi_note {
        std::uint32_t start;
        std::uint8_t key;
        std::uint8_t velocity;
    };

    // Reads the single track of a format 0 file back into notes and tempo changes.
    bool read_midi(const std::vector<std::uint8_t> &midi, std::vector<midi_note> &notes, std::vector<std::uint32_t> &tempos) {
        if ((midi.size() < 22) || (std::string(midi.begin(), midi.begin() + 4) != "MThd") || (midi[9] != 0) || (midi[13] != 96)) {
            return false;
        }

        std::size_t pos = 22;
        std::uint32_t time = 0;

        while (pos < midi.size()) {
            std::uint32_t delta = 0;
            std::uint8_t byte = 0;

            do {
                byte = midi[pos++];
                delta = (delta << 7) | (byte & 0x7F);
            } while (byte & 0x80);

            time += delta;

            const std::uint8_t status = midi[pos];
            if (status == 0xFF) {
                if (midi[pos + 1] == 0x2F) {
                    return (pos + 3) == midi.size();
                }

                if (midi[pos + 1] == 0x51) {
                    tempos.push_back((midi[pos + 3] << 16) | (midi[pos + 4] << 8) | midi[pos + 5]);
                }

                pos += 3 + midi[pos + 2];
            } else if ((status & 0xF0) == 0xC0) {
                pos += 2;
            } else {
                if ((status & 0xF0) == 0x90) {
                    notes.push_back({ time, midi[pos + 1], midi[pos + 2] });
                }

                pos += 3;
            }
        }

        return false;
    }
}

TEST_CASE("Nokia ringing tones become MIDI files", "[audio]") {
    const std::vector<std::uint8_t> ringtone = make_ringtone();
    REQUIRE(eka2l1::drivers::is_nokia_ringtone(ringtone.data(), ringtone.size()));

    std::vector<std::uint8_t> midi;
    REQUIRE(eka2l1::drivers::nokia_ringtone_to_midi(ringtone.data(), ringtone.size(), midi));

    std::vector<midi_note> notes;
    std::vector<std::uint32_t> tempos;
    REQUIRE(read_midi(midi, notes, tempos));

    // The default tempo is 63 bpm until the song sets 125.
    REQUIRE(tempos == std::vector<std::uint32_t>{ 60000000 / 63, 60000000 / 125 });

    // A of the default second scale is 880 Hz; each quarter starts one beat after the last.
    REQUIRE(notes.size() == 3);
    for (std::size_t i = 0; i < notes.size(); i++) {
        REQUIRE(notes[i].start == i * 96);
        REQUIRE(notes[i].key == 81);
        REQUIRE(notes[i].velocity == 127);
    }
}

TEST_CASE("Sounds that are not whole ringing tones are left to the other decoders", "[audio]") {
    const std::uint8_t wave[] = { 'R', 'I', 'F', 'F', 0x24, 0, 0, 0, 'W', 'A', 'V', 'E' };
    REQUIRE_FALSE(eka2l1::drivers::is_nokia_ringtone(wave, sizeof(wave)));

    const std::vector<std::uint8_t> ringtone = make_ringtone();
    std::vector<std::uint8_t> midi;
    REQUIRE_FALSE(eka2l1::drivers::nokia_ringtone_to_midi(ringtone.data(), ringtone.size() - 3, midi));
}
