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
#include <drivers/audio/backend/dsp_shared.h>
#include <drivers/audio/stream.h>

TEST_CASE("Audio frame positions use frames rather than channel samples", "[audio]") {
    using eka2l1::drivers::frames_to_microseconds;

    REQUIRE(frames_to_microseconds(48000, 48000) == 1000000);
    REQUIRE(frames_to_microseconds(22050, 44100) == 500000);
    REQUIRE(frames_to_microseconds(1234, 0) == 0);
}

namespace {
    // Only the counters matter here; nothing reaches a driver.
    struct counting_output_stream : public eka2l1::drivers::dsp_output_stream_shared {
        explicit counting_output_stream(const std::uint32_t freq, const std::uint8_t channels,
            const std::size_t samples)
            : dsp_output_stream_shared(nullptr) {
            freq_ = freq;
            channels_ = channels;
            samples_played_.store(samples);
        }

        ~counting_output_stream() override {
            shutdown_stream();
        }

        bool decode_data(std::vector<std::uint8_t> &) override {
            return false;
        }

        void queue_data_decode(const std::uint8_t *, const std::size_t) override {
        }

        void get_supported_formats(std::vector<eka2l1::drivers::four_cc> &) override {
        }
    };
}

// CMdaAudioOutputStream::Position() is "the current position within the stream in
// microseconds" (mdaaudiooutputstream.h), so a second of audio is a second whatever
// the channel count, and a second of 16-bit stereo at 16 kHz is 64000 bytes.
TEST_CASE("Stream position is playback time rather than channel samples", "[audio]") {
    counting_output_stream mono(16000, 1, 16000);
    counting_output_stream stereo(16000, 2, 32000);

    REQUIRE(mono.position() == 1000000);
    REQUIRE(stereo.position() == 1000000);

    REQUIRE(mono.bytes_rendered() == 32000);
    REQUIRE(stereo.bytes_rendered() == 64000);

    counting_output_stream unconfigured(0, 0, 0);
    REQUIRE(unconfigured.position() == 0);
}
