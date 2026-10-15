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

#include <drivers/audio/clip_writer.h>
#include <drivers/audio/player.h>

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace eka2l1::drivers {
    std::unique_ptr<clip_writer> new_clip_writer_amr(const std::string &path);
    std::unique_ptr<clip_writer> new_clip_writer_aac_mp4(const std::string &path);

    static bool contains(const std::vector<std::uint32_t> &values, const std::uint32_t value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    bool clip_writer::set_sample_rate(const std::uint32_t sample_rate) {
        if ((sample_rate != format_.sample_rate) && (has_existing_audio_ || !contains(supported_sample_rates(), sample_rate))) {
            return false;
        }

        format_.sample_rate = sample_rate;
        return true;
    }

    bool clip_writer::set_channel_count(const std::uint32_t channels) {
        if ((channels != format_.channels) && (has_existing_audio_ || !contains(supported_channel_counts(), channels))) {
            return false;
        }

        format_.channels = channels;
        return true;
    }

    bool clip_writer::set_bit_rate(const std::uint32_t bit_rate) {
        if ((bit_rate != format_.bit_rate) && !contains(supported_bit_rates(), bit_rate)) {
            return false;
        }

        format_.bit_rate = bit_rate;
        return true;
    }

    static std::uint64_t file_size(std::FILE *file) {
        if (std::fseek(file, 0, SEEK_END) != 0) {
            return 0;
        }

        const long size = std::ftell(file);
        return (size < 0) ? 0 : static_cast<std::uint64_t>(size);
    }

    static std::uint32_t read_le32(const std::uint8_t *data) {
        return data[0] | (data[1] << 8) | (data[2] << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
    }

    static std::uint16_t read_le16(const std::uint8_t *data) {
        return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    }

    static void write_le32(std::uint8_t *out, const std::uint32_t value) {
        for (int i = 0; i < 4; i++) {
            out[i] = static_cast<std::uint8_t>(value >> (i * 8));
        }
    }

    static void write_le16(std::uint8_t *out, const std::uint16_t value) {
        out[0] = static_cast<std::uint8_t>(value);
        out[1] = static_cast<std::uint8_t>(value >> 8);
    }

    // 16-bit PCM in a canonical 44-byte RIFF header with the data chunk last, so appending only
    // means writing samples at the end and patching the two sizes.
    class clip_writer_wav : public clip_writer {
        static constexpr std::size_t HEADER_SIZE = 44;

        std::FILE *file_ = nullptr;

        bool write_header() {
            const std::uint32_t data_size = static_cast<std::uint32_t>(total_frames() * format_.channels * sizeof(std::int16_t));
            const std::uint16_t block_align = static_cast<std::uint16_t>(format_.channels * sizeof(std::int16_t));

            std::uint8_t header[HEADER_SIZE];
            std::memcpy(header, "RIFF", 4);
            write_le32(header + 4, 36 + data_size);
            std::memcpy(header + 8, "WAVEfmt ", 8);
            write_le32(header + 16, 16);
            write_le16(header + 20, 1);
            write_le16(header + 22, static_cast<std::uint16_t>(format_.channels));
            write_le32(header + 24, format_.sample_rate);
            write_le32(header + 28, format_.sample_rate * block_align);
            write_le16(header + 32, block_align);
            write_le16(header + 34, 16);
            std::memcpy(header + 36, "data", 4);
            write_le32(header + 40, data_size);

            return (std::fseek(file_, 0, SEEK_SET) == 0) && (std::fwrite(header, 1, HEADER_SIZE, file_) == HEADER_SIZE);
        }

        void update_bit_rate() {
            format_.bit_rate = format_.sample_rate * format_.channels * 16;
        }

    public:
        explicit clip_writer_wav(const std::string &path)
            : clip_writer(path) {
            format_.sample_rate = 8000;
            format_.channels = 1;
            update_bit_rate();
        }

        ~clip_writer_wav() override {
            end();
        }

        // Returns false if the clip holds audio this writer cannot append to.
        bool probe() {
            std::FILE *file = common::open_c_file(path_, "rb");
            if (!file) {
                return true;
            }

            const std::uint64_t size = file_size(file);
            std::uint8_t header[HEADER_SIZE];

            bool readable = (size >= HEADER_SIZE) && (std::fseek(file, 0, SEEK_SET) == 0) &&
                (std::fread(header, 1, HEADER_SIZE, file) == HEADER_SIZE);
            std::fclose(file);

            if (size == 0) {
                return true;
            }

            if (!readable || (std::memcmp(header, "RIFF", 4) != 0) || (std::memcmp(header + 8, "WAVEfmt ", 8) != 0) ||
                (read_le32(header + 16) != 16) || (read_le16(header + 20) != 1) || (read_le16(header + 34) != 16) ||
                (std::memcmp(header + 36, "data", 4) != 0)) {
                return false;
            }

            const std::uint16_t channels = read_le16(header + 22);
            const std::uint32_t sample_rate = read_le32(header + 24);
            const std::uint32_t data_size = read_le32(header + 40);

            if ((channels == 0) || (sample_rate == 0) || (HEADER_SIZE + data_size != size)) {
                return false;
            }

            format_.sample_rate = sample_rate;
            format_.channels = channels;
            update_bit_rate();

            has_existing_audio_ = true;
            existing_frames_ = data_size / (channels * sizeof(std::int16_t));

            return true;
        }

        std::uint32_t codec_four_cc() const override {
            return AUDIO_PCM16_CODEC_4CC;
        }

        std::vector<std::uint32_t> supported_sample_rates() const override {
            if (has_existing_audio_) {
                return { format_.sample_rate };
            }

            return { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 };
        }

        std::vector<std::uint32_t> supported_channel_counts() const override {
            if (has_existing_audio_) {
                return { format_.channels };
            }

            return { 1, 2 };
        }

        // The bit rate follows from the sample rate and channel count.
        std::vector<std::uint32_t> supported_bit_rates() const override {
            return { format_.bit_rate };
        }

        bool set_sample_rate(const std::uint32_t sample_rate) override {
            if (!clip_writer::set_sample_rate(sample_rate)) {
                return false;
            }

            update_bit_rate();
            return true;
        }

        bool set_channel_count(const std::uint32_t channels) override {
            if (!clip_writer::set_channel_count(channels)) {
                return false;
            }

            update_bit_rate();
            return true;
        }

        bool begin() override {
            if (file_) {
                return false;
            }

            file_ = common::open_c_file(path_, "r+b");
            if (!file_) {
                file_ = common::open_c_file(path_, "w+b");
            }

            if (!file_) {
                return false;
            }

            // The clip may have been rewritten since it was probed.
            const std::uint64_t expected_size = has_existing_audio_ ? HEADER_SIZE + existing_frames_ * format_.channels * sizeof(std::int16_t) : 0;
            if ((file_size(file_) != expected_size) || (!has_existing_audio_ && !write_header())) {
                std::fclose(file_);
                file_ = nullptr;

                return false;
            }

            return true;
        }

        bool write(const std::int16_t *samples, const std::size_t frames) override {
            if (!file_) {
                return false;
            }

            const std::size_t count = frames * format_.channels;
            bool written = (std::fseek(file_, 0, SEEK_END) == 0) && (std::fwrite(samples, sizeof(std::int16_t), count, file_) == count);

            written_frames_ += frames;
            written = write_header() && written;

            std::fflush(file_);
            return written;
        }

        bool end() override {
            if (!file_) {
                return false;
            }

            std::fclose(file_);
            file_ = nullptr;

            has_existing_audio_ = has_existing_audio_ || (written_frames_ != 0);
            existing_frames_ += written_frames_;
            written_frames_ = 0;

            return true;
        }
    };

    std::unique_ptr<clip_writer> new_clip_writer(const std::string &path) {
        const std::string extension = common::lowercase_string(eka2l1::path_extension(path));

        if (extension == ".amr") {
            return new_clip_writer_amr(path);
        }

        if ((extension == ".mp4") || (extension == ".m4a")) {
            return new_clip_writer_aac_mp4(path);
        }

        auto writer = std::make_unique<clip_writer_wav>(path);
        return writer->probe() ? std::move(writer) : nullptr;
    }

#ifndef EKA2L1_HAS_FFMPEG
    std::unique_ptr<clip_writer> new_clip_writer_amr(const std::string &path) {
        LOG_ERROR(DRIVER_AUD, "Recording AMR into {} needs FFmpeg, which this build lacks", path);
        return nullptr;
    }

    std::unique_ptr<clip_writer> new_clip_writer_aac_mp4(const std::string &path) {
        LOG_ERROR(DRIVER_AUD, "Recording AAC into {} needs FFmpeg, which this build lacks", path);
        return nullptr;
    }
#endif
}
