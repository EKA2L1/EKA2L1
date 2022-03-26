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
#include <mutex>
#include <queue>
#include <vector>

#include <drivers/audio/common.h>
#include <drivers/audio/player.h>
#include <drivers/audio/stream.h>

namespace eka2l1::drivers {
    class audio_driver;

    struct player_shared : public player {
    private:
        audio_driver *aud_;

    protected:
        std::string url_;

        std::vector<std::uint8_t> data_;

        std::uint32_t freq_;
        std::uint32_t encoding_;
        std::uint32_t channels_;
        std::uint32_t format_;

        std::size_t data_pointer_;
        std::uint32_t flags_;

        std::int32_t repeat_left_;
        std::int64_t silence_micros_;

        std::uint64_t pos_in_us_;

        bool use_push_new_data_;

        virtual bool is_ready_to_play() = 0;

        std::vector<player_metadata> metadatas_;
        std::unique_ptr<audio_output_stream> output_stream_;

    public:
        explicit player_shared(audio_driver *driver);
        ~player_shared() override;

        virtual bool make_backend_source() = 0;

        virtual void reset_request() = 0;
        virtual void get_more_data() = 0;
        virtual bool set_position_for_custom_format(const std::uint64_t pos_in_us) = 0;
        std::size_t data_supply_callback(std::int16_t *data, std::size_t size);

        bool play() override;
        bool stop() override;
        void pause() override;

        bool notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size) override;
        void clear_notify_done() override;

        void set_repeat(const std::int32_t repeat_times, const std::int64_t silence_intervals_micros) override;
        void set_position(const std::uint64_t pos_in_us) override;

        bool set_dest_freq(const std::uint32_t freq) override;
        bool set_dest_channel_count(const std::uint32_t cn) override;
        bool set_dest_encoding(const std::uint32_t enc) override;
        bool set_volume(const std::uint32_t vol) override;
        void set_dest_container_format(const std::uint32_t confor) override;

        std::uint64_t position() const override;
        std::uint32_t get_dest_freq() override;
        std::uint32_t get_dest_channel_count() override;
        std::uint32_t get_dest_encoding() override;

        bool is_playing() const override {
            return (output_stream_ && output_stream_->is_playing());
        }
    };
}