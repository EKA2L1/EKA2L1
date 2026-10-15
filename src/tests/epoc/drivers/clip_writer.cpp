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
#include <drivers/audio/clip_writer.h>
#include <drivers/audio/player.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#ifdef EKA2L1_HAS_FFMPEG
#include <drivers/audio/backend/ffmpeg/player_ffmpeg.h>

extern "C" {
#include <libavformat/avformat.h>
}
#endif

namespace {
    std::vector<std::int16_t> make_tone(const std::uint32_t sample_rate, const std::uint32_t channels, const std::size_t frames) {
        std::vector<std::int16_t> samples(frames * channels);

        for (std::size_t i = 0; i < frames; i++) {
            const double value = 8000.0 * std::sin(i * 2.0 * 3.141592653589793 * 440.0 / sample_rate);

            for (std::uint32_t c = 0; c < channels; c++) {
                samples[i * channels + c] = static_cast<std::int16_t>(value);
            }
        }

        return samples;
    }

    std::vector<std::uint8_t> read_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    struct temp_clip {
        std::filesystem::path path;

        explicit temp_clip(const char *name)
            : path(std::filesystem::temp_directory_path() / name) {
            std::filesystem::remove(path);
        }

        ~temp_clip() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };
}

TEST_CASE("WAV recordings keep the format of the clip they append to", "[audio][record]") {
    temp_clip clip("eka2l1_clip_writer_test.wav");

    auto writer = eka2l1::drivers::new_clip_writer(clip.path.string());
    REQUIRE(writer);
    REQUIRE(writer->codec_four_cc() == eka2l1::drivers::AUDIO_PCM16_CODEC_4CC);
    REQUIRE(writer->set_sample_rate(16000));
    REQUIRE(writer->set_channel_count(2));
    REQUIRE(writer->format().bit_rate == 16000 * 2 * 16);

    const std::vector<std::int16_t> tone = make_tone(16000, 2, 1600);

    REQUIRE(writer->begin());
    REQUIRE(writer->write(tone.data(), 1600));
    REQUIRE(writer->end());

    const std::vector<std::uint8_t> data = read_file(clip.path);
    REQUIRE(data.size() == 44 + 1600 * 4);

    writer = eka2l1::drivers::new_clip_writer(clip.path.string());
    REQUIRE(writer);
    REQUIRE(writer->format().sample_rate == 16000);
    REQUIRE(writer->format().channels == 2);
    REQUIRE(writer->total_frames() == 1600);
    REQUIRE_FALSE(writer->set_sample_rate(8000));
    REQUIRE(writer->supported_channel_counts() == std::vector<std::uint32_t>{ 2 });

    REQUIRE(writer->begin());
    REQUIRE(writer->write(tone.data(), 800));
    REQUIRE(writer->end());
    REQUIRE(read_file(clip.path).size() == 44 + 2400 * 4);
}

TEST_CASE("A clip in another format is not recorded over", "[audio][record]") {
    temp_clip clip("eka2l1_clip_writer_test_foreign.amr");

    {
        std::ofstream stream(clip.path, std::ios::binary);
        stream << "RIFF not an AMR clip";
    }

    REQUIRE_FALSE(eka2l1::drivers::new_clip_writer(clip.path.string()));
}

#ifdef EKA2L1_HAS_FFMPEG
namespace {
    // RFC 4867 section 5.3 storage format: speech bytes after the header byte, by frame type.
    std::size_t count_amr_frames(const std::vector<std::uint8_t> &data, std::vector<std::uint8_t> *types = nullptr) {
        static const std::size_t sizes[16] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };
        std::size_t offset = 6;
        std::size_t frames = 0;

        while (offset < data.size()) {
            const std::uint8_t type = (data[offset] >> 3) & 0xF;
            if (types) {
                types->push_back(type);
            }

            offset += 1 + sizes[type];
            frames++;
        }

        REQUIRE(offset == data.size());
        return frames;
    }
}

TEST_CASE("AMR recordings are RFC 4867 storage files that later takes append to", "[audio][record]") {
    temp_clip clip("eka2l1_clip_writer_test.amr");

    auto writer = eka2l1::drivers::new_clip_writer(clip.path.string());
    REQUIRE(writer);
    REQUIRE(writer->codec_four_cc() == eka2l1::drivers::AUDIO_AMR_CODEC_4CC);
    REQUIRE(writer->format().sample_rate == 8000);
    REQUIRE(writer->format().channels == 1);
    REQUIRE_FALSE(writer->set_sample_rate(16000));
    REQUIRE_FALSE(writer->set_bit_rate(13000));
    REQUIRE(writer->supported_bit_rates().size() == 8);

    const std::vector<std::int16_t> tone = make_tone(8000, 1, 8000);

    // Feed it in pieces that do not line up with 160-sample frames.
    REQUIRE(writer->begin());
    REQUIRE(writer->write(tone.data(), 1000));
    REQUIRE(writer->write(tone.data() + 1000, 7000));
    REQUIRE(writer->end());

    // The encoder's priming samples push the last speech into a 51st frame.
    std::vector<std::uint8_t> data = read_file(clip.path);
    REQUIRE(data.size() > 6);
    REQUIRE(std::memcmp(data.data(), "#!AMR\n", 6) == 0);
    REQUIRE(count_amr_frames(data) == 51);
    REQUIRE(writer->total_frames() == 51 * 160);

    // A second take at another bit rate, through a writer that probes the existing clip.
    writer = eka2l1::drivers::new_clip_writer(clip.path.string());
    REQUIRE(writer);
    REQUIRE(writer->total_frames() == 51 * 160);
    REQUIRE(writer->format().bit_rate == 12200);
    REQUIRE(writer->set_bit_rate(4750));

    REQUIRE(writer->begin());
    REQUIRE(writer->write(tone.data(), 100));
    REQUIRE(writer->end());

    std::vector<std::uint8_t> types;
    data = read_file(clip.path);
    REQUIRE(count_amr_frames(data, &types) == 52);
    REQUIRE(types.front() == 7);
    REQUIRE(types.back() == 0);
    REQUIRE(writer->total_frames() == 52 * 160);
    REQUIRE(writer->format().bit_rate == 4750);
}

TEST_CASE("AAC recordings are MP4 clips that later takes append to", "[audio][record]") {
    temp_clip clip("eka2l1_clip_writer_test.mp4");

    auto writer = eka2l1::drivers::new_clip_writer(clip.path.string());
    REQUIRE(writer);
    REQUIRE(writer->codec_four_cc() == eka2l1::drivers::AUDIO_AAC_CODEC_4CC);
    REQUIRE(writer->set_sample_rate(16000));
    REQUIRE(writer->set_channel_count(2));
    REQUIRE(writer->set_bit_rate(32000));

    const std::vector<std::int16_t> tone = make_tone(16000, 2, 16000);

    REQUIRE(writer->begin());
    REQUIRE(writer->write(tone.data(), 5000));
    REQUIRE(writer->write(tone.data() + 5000 * 2, 11000));
    REQUIRE(writer->end());

    writer = eka2l1::drivers::new_clip_writer(clip.path.string());
    REQUIRE(writer);
    REQUIRE(writer->format().sample_rate == 16000);
    REQUIRE(writer->format().channels == 2);
    REQUIRE(writer->total_frames() == 16000);
    REQUIRE_FALSE(writer->set_sample_rate(48000));

    REQUIRE(writer->begin());
    REQUIRE(writer->write(tone.data(), 8000));
    REQUIRE(writer->end());

    // Each appended take carries the encoder's priming samples.
    const std::uint64_t total = writer->total_frames();
    REQUIRE(total >= 24000);
    REQUIRE(total <= 24000 + 2048);

    AVFormatContext *input = nullptr;
    REQUIRE(avformat_open_input(&input, clip.path.string().c_str(), nullptr, nullptr) >= 0);
    REQUIRE(avformat_find_stream_info(input, nullptr) >= 0);
    REQUIRE(input->nb_streams == 1);
    REQUIRE(input->streams[0]->codecpar->codec_id == AV_CODEC_ID_AAC);
    REQUIRE(input->streams[0]->codecpar->sample_rate == 16000);

    const double seconds = input->duration / static_cast<double>(AV_TIME_BASE);
    avformat_close_input(&input);

    REQUIRE(seconds == Approx(total / 16000.0).margin(0.07));
}

TEST_CASE("AAC recordings play back from the first sample", "[audio][record]") {
    temp_clip clip("eka2l1_clip_writer_test_play.mp4");

    {
        auto writer = eka2l1::drivers::new_clip_writer(clip.path.string());
        REQUIRE(writer);

        const std::vector<std::int16_t> tone = make_tone(48000, 1, 48000);
        REQUIRE(writer->begin());
        REQUIRE(writer->write(tone.data(), tone.size()));
        REQUIRE(writer->end());
    }

    eka2l1::drivers::player_ffmpeg player(nullptr);
    REQUIRE(player.open_url(clip.path.string()));

    std::vector<std::int16_t> output(48000);
    REQUIRE(player.data_supply_callback(output.data(), output.size()) >= 47000);
}

TEST_CASE("AMR recordings report their duration and play back in full", "[audio][record]") {
    temp_clip clip("eka2l1_clip_writer_test_play.amr");

    {
        auto writer = eka2l1::drivers::new_clip_writer(clip.path.string());
        REQUIRE(writer);

        const std::vector<std::int16_t> tone = make_tone(8000, 1, 8000);
        REQUIRE(writer->begin());
        REQUIRE(writer->write(tone.data(), tone.size()));
        REQUIRE(writer->end());
    }

    eka2l1::drivers::player_ffmpeg player(nullptr);
    REQUIRE(player.open_url(clip.path.string()));
    REQUIRE(player.duration() == 51 * 20000);

    std::vector<std::int16_t> output(9000);
    REQUIRE(player.data_supply_callback(output.data(), output.size()) == 51 * 160);
}
#endif
