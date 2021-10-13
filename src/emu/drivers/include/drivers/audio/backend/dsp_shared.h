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

#include <drivers/audio/audio.h>
#include <drivers/audio/dsp.h>

#include <common/container.h>

#include <mutex>
#include <vector>

namespace eka2l1::drivers {
    using dsp_buffer = std::vector<std::uint8_t>;


    struct dsp_output_stream_shared : public dsp_output_stream {
    protected:
        static constexpr std::size_t RING_BUFFER_MAX_SAMPLE_COUNT = 0x20000;

        drivers::audio_driver *aud_;
        std::unique_ptr<drivers::audio_output_stream> stream_;

        common::ring_buffer<std::uint16_t, RING_BUFFER_MAX_SAMPLE_COUNT> buffer_;

        std::mutex callback_lock_;
        std::size_t avg_frame_count_;

        bool virtual_stop;
        bool more_requested;

    protected:
        virtual bool internal_decode_running_out();

    public:
        explicit dsp_output_stream_shared(drivers::audio_driver *aud);
        ~dsp_output_stream_shared() override;

        virtual bool decode_data(std::vector<std::uint8_t> &dest) = 0;
        virtual void queue_data_decode(const std::uint8_t *original, const std::size_t original_size) = 0;

        std::size_t data_callback(std::int16_t *buffer, const std::size_t frame_count);

        bool write(const std::uint8_t *data, const std::uint32_t data_size) override;

        void volume(const std::uint32_t new_volume) override;
        bool set_properties(const std::uint32_t freq, const std::uint8_t channels) override;

        void register_callback(dsp_stream_notification_type nof_type, dsp_stream_notification_callback callback,
            void *userdata) override;

        void *get_userdata(dsp_stream_notification_type nof_type) override;

        virtual bool start() override;
        virtual bool stop() override;

        virtual std::uint64_t real_time_position() override;
        std::uint64_t position() override;

        virtual bool is_playing() const override {
            return !virtual_stop;
        }
    };
}