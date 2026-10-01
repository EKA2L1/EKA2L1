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

#ifdef EKA2L1_HAS_FFMPEG
#include <common/buffer.h>
#include <drivers/audio/backend/ffmpeg/player_ffmpeg.h>

namespace {
    struct memory_audio_stream : eka2l1::common::rw_stream {
        std::vector<std::uint8_t> data = std::vector<std::uint8_t>(65536, 0);
        eka2l1::common::ro_buf_stream reader{data.data(), data.size()};
        std::uint64_t read(void *buffer, const std::uint64_t size) override {
            return reader.read(buffer, size);
        }
        void seek(const std::int64_t amount, eka2l1::common::seek_where where) override {
            reader.seek(amount, where);
        }
        std::uint64_t tell() override { return reader.tell(); }
        std::uint64_t left() override { return reader.left(); }
        std::uint64_t size() override { return data.size(); }
    };
}

TEST_CASE("Invalid custom audio survives probing and repeated cleanup", "[audio]") {
    memory_audio_stream stream;
    eka2l1::drivers::player_ffmpeg player(nullptr);
    REQUIRE_FALSE(player.open_custom(&stream));
    stream.seek(0, eka2l1::common::seek_where::beg);
    REQUIRE_FALSE(player.open_custom(&stream));
}
TEST_CASE("EPOC Record custom audio decodes and can be reopened", "[audio]") {
    memory_audio_stream stream;
    auto word = [&](std::size_t offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i) stream.data[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    };
    word(0, 0x10000037); word(4, 0x1000006d); word(8, 0x1000007e);
    word(12, 0x5508accf); word(16, 20); stream.data[20] = 4;
    word(21, 0x10000052); word(25, 52); word(29, 0x10000089); word(33, 37);
    word(52, 4000); word(56, 0); word(60, 0); word(64, 0); word(68, 4000);
    std::fill(stream.data.begin() + 72, stream.data.begin() + 4072, 0xd5);
    eka2l1::drivers::player_ffmpeg player(nullptr);
    REQUIRE(player.open_custom(&stream));
    REQUIRE(player.duration() == 500000);
    REQUIRE(player.open_custom(&stream));
    REQUIRE(player.duration() == 500000);
    player.set_repeat(1, 1000);
    std::vector<std::int16_t> output(4500, -1);
    REQUIRE(player.data_supply_callback(output.data(), output.size()) == output.size());
    REQUIRE(std::all_of(output.begin(), output.begin() + 4000, [](auto sample) { return sample == 8; }));
    REQUIRE(std::all_of(output.begin() + 4000, output.begin() + 4008, [](auto sample) { return sample == 0; }));
    REQUIRE(std::all_of(output.begin() + 4008, output.end(), [](auto sample) { return sample == 8; }));
}

TEST_CASE("PCM encoder accepts unrestricted sample rates and channel layouts", "[audio]") {
    eka2l1::drivers::player_ffmpeg player(nullptr);
    REQUIRE(player.set_dest_encoding(eka2l1::drivers::AUDIO_PCM16_CODEC_4CC));
    REQUIRE(player.set_dest_freq(8000));
    REQUIRE(player.set_dest_channel_count(1));
    REQUIRE(player.get_dest_freq() == 8000);
    REQUIRE(player.get_dest_channel_count() == 1);
    REQUIRE(player.set_dest_freq(44100));
    REQUIRE(player.set_dest_channel_count(2));
    REQUIRE(player.get_dest_freq() == 44100);
    REQUIRE(player.get_dest_channel_count() == 2);
    REQUIRE_FALSE(player.set_dest_freq(0));
    REQUIRE_FALSE(player.set_dest_channel_count(0));
}
#endif
