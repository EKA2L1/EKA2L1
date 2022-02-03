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

#include <common/vecx.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::drivers {
    class audio_driver;

    /**
     * @brief Class allow the use of streaming video.
     * 
     * With EKA2L1's usage, the audio will be directly handled by this class, while the image will be
     * perodically provided through callback with the interval depends on the set/provided FPS.
     * 
     * The reason for image callback varies from direct display to mixed display in situations like windowing.
     */
    class video_player {
    public:
        /**
         * @brief Callback that will be invoked when the play of the video is completed.
         * 
         * Note that this will not be called on stop. Second argument provides the error code of the play.
         * 0 indicates that the play is done and no problem.
         */
        using play_complete_callback = std::function<void(void*, const int)>;

        /**
         * @brief Callback that will be invoked when a new frame is available.
         * 
         * The frame data is in RGBA format. Any backend must convert to this format before calling this
         * function. Second parameter provided the pointer to the data buffer, and the thrid parameter provides
         * the size of the buffer.
         * 
         * YUV is not preferred due to some platforms not having native support for it.
         */
        using image_frame_available_callback = std::function<void(void*, const std::uint8_t*, const std::size_t)>;

    protected:
        play_complete_callback play_complete_callback_;
        void *play_complete_callback_userdata_;

        image_frame_available_callback image_frame_available_callback_;
        void *image_frame_available_callback_userdata_;

    public:
        explicit video_player()
            : play_complete_callback_(nullptr)
            , play_complete_callback_userdata_(nullptr)
            , image_frame_available_callback_(nullptr)
            , image_frame_available_callback_userdata_(nullptr) {
        }

        virtual ~video_player() = default;

        virtual bool open_file(const std::string &path) = 0;
        virtual bool open_custom_io(common::ro_stream &stream) = 0;
        virtual void close() = 0;

        virtual void play(const std::uint64_t *us_range) = 0;
        virtual void pause() = 0;
        virtual void stop() = 0;

        virtual std::uint32_t max_volume() const = 0;
        virtual std::uint32_t volume() const = 0;
        virtual bool set_volume(const std::uint32_t volume) = 0;

        virtual std::uint64_t duration() const = 0;
        virtual std::uint64_t position() const = 0;
        virtual void set_position(const std::uint64_t pos) = 0;
        virtual eka2l1::vec2 get_video_size() const = 0;

        virtual void set_fps(const float fps) = 0;
        virtual float get_fps() const = 0;

        virtual std::uint32_t audio_bitrate() const = 0;
        virtual std::uint32_t video_bitrate() const = 0;

        void set_image_frame_available_callback(image_frame_available_callback callback, void *userdata) {
            image_frame_available_callback_ = callback;
            image_frame_available_callback_userdata_ = userdata;
        }

        void set_play_complete_callback(play_complete_callback callback, void *userdata) {
            play_complete_callback_ = callback;
            play_complete_callback_userdata_ = userdata;
        }
    };

    using video_player_instance = std::unique_ptr<video_player>;

    video_player_instance new_best_video_player(audio_driver *drv);
}