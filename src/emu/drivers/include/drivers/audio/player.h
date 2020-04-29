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

#include <functional>
#include <memory>
#include <string>

#include <vector>

namespace eka2l1::drivers {
    class audio_driver;
    using finish_callback = std::function<void(std::uint8_t *)>;

    enum player_audio_encoding {
        player_audio_encoding_pcm8 = 0,
        player_audio_encoding_pcm16 = 1,
        player_audio_encoding_custom = 2
    };

    struct player {
    protected:
        std::uint32_t volume_;
        std::int32_t balance_;

        std::vector<std::uint8_t> userdata_;
        finish_callback callback_;

    public:
        explicit player()
            : volume_(50)
            , balance_(0)
            , callback_(nullptr) {
        }

        virtual ~player() = default;

        virtual bool play() = 0;
        virtual bool stop() = 0;

        virtual bool queue_url(const std::string &url) = 0;

        virtual bool queue_data(const char *raw_data, const std::size_t data_size,
            const std::uint32_t encoding_type, const std::uint32_t frequency,
            const std::uint32_t channels)
            = 0;

        virtual std::uint32_t max_volume() const {
            return 100;
        }

        virtual std::uint32_t volume() const {
            return volume_;
        }

        virtual bool set_volume(const std::uint32_t vol) {
            if (vol > max_volume()) {
                return false;
            }

            volume_ = vol;
            return true;
        }

        virtual std::int32_t balance() const {
            return balance_;
        }

        virtual bool set_balance(const std::int32_t balance) {
            if (balance > 100 || balance < -100) {
                return false;
            }

            balance_ = balance;
            return true;
        }

        virtual bool notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size);
        virtual std::uint8_t *get_notify_userdata(std::size_t *size_of_data);

        virtual finish_callback get_finish_callback() {
            return callback_;
        }

        virtual void clear_notify_done() {
            callback_ = nullptr;
            userdata_.clear();
        }

        virtual void set_repeat(const std::int32_t repeat_times, const std::uint64_t silence_intervals_micros) = 0;
    };

    enum player_type {
        player_type_wmf = 0,
        player_type_ffmpeg = 1
    };

    std::unique_ptr<player> new_audio_player(audio_driver *aud, const player_type type);
    player_type get_suitable_player_type();
}