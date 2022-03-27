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

#include <drivers/audio/stream.h>
#include <drivers/audio/player.h>
#include <drivers/driver.h>

#include <common/container.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>

namespace eka2l1::drivers {
    using master_audio_volume_change_callback = std::function<void(const std::uint32_t old, const std::uint32_t newv)>;
    using bank_change_callback = std::function<void(const midi_bank_type type, const std::string &new_path)>;

    class audio_driver : public driver {
        friend struct audio_output_stream;

    private:
        common::identity_container<master_audio_volume_change_callback> master_volume_change_callbacks_;
        common::identity_container<bank_change_callback> bank_change_callbacks_;

        // MIDIs...
        player_type preferred_midi_backend_;
        std::array<std::string, MIDI_BANK_TYPE_MAX> midi_banks_;

        std::uint32_t master_volume_ = 100;
        bool suspend_ = false;

        std::mutex lock_;

        std::size_t add_master_volume_change_callback(master_audio_volume_change_callback callback);
        bool remove_master_volume_change_callback(const std::size_t handle);

    public:
        explicit audio_driver(const std::uint32_t initial_master_volume = 100, const player_type preferred_midi_backend = player_type_tsf);
        virtual ~audio_driver() {}

        void run() override {}
        void abort() override {}

        /**
         * \brief Create a signed 16-bit LE audio output stream.
         * 
         * \param sample_rate       The target sample rate of output stream.
         * \param channels          The number of channels of the stream.
         * \param callback          The callback that the stream will use to retrive data.
         * 
         * \returns Instance to the stream on success.
         * 
         * \see     native_sample_rate
         */
        virtual std::unique_ptr<audio_output_stream> new_output_stream(const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback)
            = 0;

        virtual std::uint32_t native_sample_rate() = 0;

        std::uint32_t master_volume() const {
            return master_volume_;
        }

        bool suspending() const {
            return suspend_;
        }

        virtual void suspend() {
            suspend_ = true;
        }

        virtual void resume() {
            suspend_ = false;
        }

        void master_volume(const std::uint32_t value);

        std::vector<player_type> get_suitable_player_types(const std::string &url);
        void set_preferred_midi_backend(const player_type type);

        std::optional<std::string> get_bank_path(const midi_bank_type type) const;
        void set_bank_path(const midi_bank_type type, const std::string &path);

        std::size_t add_bank_change_callback(bank_change_callback callback);
        bool remove_bank_change_callback(const std::size_t handle);
    };

    enum class audio_driver_backend {
        cubeb
    };

    using audio_driver_instance = std::unique_ptr<audio_driver>;
    audio_driver_instance make_audio_driver(const audio_driver_backend backend, const std::uint32_t initial_master_vol = 100,
         const player_type preferred_midi_backend = player_type_tsf);
}
