/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <atomic>
#include <thread>

#include <common/sync.h>
#include <common/queue.h>
#include <common/container.h>

#include <drivers/video/video.h>
#include <drivers/audio/stream.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

namespace eka2l1::drivers {
    class video_player_ffmpeg : public video_player {
    private:
        std::unique_ptr<audio_output_stream> stream_;
        std::unique_ptr<std::thread> decode_thread_;
        threadsafe_cn_queue<AVPacket*> audio_packets_;

        common::ring_buffer<std::uint16_t, 0x20000> pending_samples_;
        common::event done_event_;

        std::atomic_bool should_stop_;

        audio_driver *aud_driver_;

        AVFormatContext *format_ctx_;
        AVCodecContext *audio_codec_ctx_;
        AVCodecContext *image_codec_ctx_;
        AVFrame *temp_audio_frame_;
        SwrContext *resample_context_;

        int audio_stream_index_;
        int image_stream_index_;

        std::uint32_t volume_;
        float fps_;

        eka2l1::vec2 target_size_;
        std::uint64_t last_update_us_;

        void reset_contexts();
        bool prepare_codecs();

    public:
        explicit video_player_ffmpeg(audio_driver *driver);
        ~video_player_ffmpeg() override;

        bool open_file(const std::string &path) override;
        bool open_custom_io(common::ro_stream &stream) override;
        void close() override;

        void play(const std::uint64_t *us_range) override;
        void pause() override;
        void stop() override;

        std::uint32_t max_volume() const override;
        std::uint32_t volume() const override;
        bool set_volume(const std::uint32_t volume) override;

        std::uint64_t duration() const override;
        std::uint64_t position() const override;
        void set_position(const std::uint64_t pos) override;

        void set_fps(const float fps) override;
        float get_fps() const override;

        std::uint32_t audio_bitrate() const override;
        std::uint32_t video_bitrate() const override;
        eka2l1::vec2 get_video_size() const override;

        std::size_t video_audio_callback(std::int16_t *output_buffer, std::size_t frames);
        void video_audio_decode_loop();
    };
}