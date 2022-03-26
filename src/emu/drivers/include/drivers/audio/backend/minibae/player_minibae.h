/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <drivers/audio/player.h>
#include <BAE_Source/Common/MiniBAE.h>

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace eka2l1::drivers {
    struct player_minibae : public player {
    private:
        BAESong song_;
        std::int32_t repeat_times_;
        bool paused_;

    public:
        explicit player_minibae(audio_driver *driver);
        ~player_minibae() override;

        void call_song_done();

        bool play() override;
        bool record() override;
        bool stop() override;
        bool crop() override;
        void pause() override;

        bool set_volume(const std::uint32_t vol) override;
        bool open_url(const std::string &url) override;
        bool open_custom(common::rw_stream *stream) override;

        bool set_dest_encoding(const std::uint32_t enc) override;
        bool set_dest_freq(const std::uint32_t freq) override;
        bool set_dest_channel_count(const std::uint32_t cn) override;
        void set_dest_container_format(const std::uint32_t confor) override;

        std::uint32_t get_dest_freq() override;
        std::uint32_t get_dest_channel_count() override;
        std::uint32_t get_dest_encoding() override;

        bool is_playing() const override;

        void set_repeat(const std::int32_t repeat_times, const std::int64_t silence_intervals_micros) override;
        void set_position(const std::uint64_t pos_in_us) override;

        std::uint64_t position() const override;

        std::int32_t get_repeat_times() const {
            return repeat_times_;
        }

        player_type get_player_type() const override {
            return player_type_minibae;
        }
    };
}