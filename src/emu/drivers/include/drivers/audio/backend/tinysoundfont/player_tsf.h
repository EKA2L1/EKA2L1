/*
 * Copyright (c) 2021 EKA2L1 Team.
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
#include <drivers/audio/stream.h>

#include <TinySoundFont/tsf.h>
#include <TinySoundFont/tml.h>

#include <common/container.h>

#include <memory>
#include <mutex>

namespace eka2l1::drivers {
    struct player_tsf : public player {
    private:
        audio_driver *driver_;

        std::unique_ptr<drivers::audio_output_stream> output_;
        std::mutex msg_lock_;

        tsf *synth_;
        tml_message *begin_msg_;
        tml_message *playing_message_;

        std::uint32_t repeat_count_;
        std::uint32_t repeat_left_;
        std::uint32_t silence_intervals_us_;

        common::ring_buffer<std::int16_t, 0x10000> buffer_queue_;
        std::vector<std::int16_t> block_buffer;

        double current_msecs_;
        std::size_t bank_change_callback_;

    protected:
        std::size_t data_callback(std::int16_t *buffer, const std::size_t frame_count);
        tsf *load_bank_from_file(const std::string &path);

    public:
        explicit player_tsf(audio_driver *driver);
        ~player_tsf() override;

        void call_song_done();
        void call_bank_change(const midi_bank_type type, const std::string &new_path);

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

        player_type get_player_type() const override {
            return player_type_tsf;
        }
    };
}