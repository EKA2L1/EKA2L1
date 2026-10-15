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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    static constexpr std::uint32_t AUDIO_AMR_CODEC_4CC = 0x524D4120;
    static constexpr std::uint32_t AUDIO_AAC_CODEC_4CC = 0x43414120;

    struct clip_record_format {
        std::uint32_t sample_rate = 0;
        std::uint32_t channels = 0;
        std::uint32_t bit_rate = 0;
    };

    /**
     * \brief Encodes captured 16-bit PCM into an audio clip file.
     *
     * Recording appends (CMdaAudioRecorderUtility::RecordL), so a non-empty clip pins the format to
     * what the file already holds, and the writer keeps its existing audio in front of new samples.
     */
    class clip_writer {
    protected:
        std::string path_;
        clip_record_format format_;

        // Whether the clip held audio when it was probed, and how many frames of it.
        bool has_existing_audio_ = false;
        std::uint64_t existing_frames_ = 0;
        std::uint64_t written_frames_ = 0;

    public:
        explicit clip_writer(const std::string &path)
            : path_(path) {
        }

        virtual ~clip_writer() = default;

        virtual std::uint32_t codec_four_cc() const = 0;
        virtual std::vector<std::uint32_t> supported_sample_rates() const = 0;
        virtual std::vector<std::uint32_t> supported_channel_counts() const = 0;
        virtual std::vector<std::uint32_t> supported_bit_rates() const = 0;

        virtual bool set_sample_rate(const std::uint32_t sample_rate);
        virtual bool set_channel_count(const std::uint32_t channels);
        virtual bool set_bit_rate(const std::uint32_t bit_rate);

        const clip_record_format &format() const {
            return format_;
        }

        // Opens the clip for appending. Fails if its existing content cannot be extended.
        virtual bool begin() = 0;
        virtual bool write(const std::int16_t *samples, const std::size_t frames) = 0;
        virtual bool end() = 0;

        std::uint64_t total_frames() const {
            return existing_frames_ + written_frames_;
        }
    };

    /**
     * \brief Create a writer for the clip at the given path, choosing the format from its extension.
     *
     * \returns Null if the extension names a format this build cannot encode, or the clip already
     *          holds audio of another format.
     */
    std::unique_ptr<clip_writer> new_clip_writer(const std::string &path);
}
