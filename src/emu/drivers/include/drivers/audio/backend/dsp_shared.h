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

#include <queue>

namespace eka2l1::drivers {
    using dsp_buffer = std::vector<std::uint8_t>;


    struct dsp_output_stream_shared : public dsp_output_stream {
    protected:
        static constexpr std::size_t RING_BUFFER_MAX_SAMPLE_COUNT = 0x20000;

        drivers::audio_driver *aud_;
        std::unique_ptr<drivers::audio_output_stream> stream_;

        common::ring_buffer<std::uint16_t, RING_BUFFER_MAX_SAMPLE_COUNT> buffer_;
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

        virtual bool start() override;
        virtual bool stop() override;

        virtual std::uint64_t real_time_position() override;
        std::uint64_t position() override;

        virtual bool is_playing() const override {
            return !virtual_stop;
        }
    };
    
    struct dsp_input_stream_shared : public dsp_input_stream {
    private:
        using input_read_request = std::pair<std::uint8_t*, std::uint32_t>;

        static constexpr std::size_t RING_BUFFER_MAX_SAMPLE_COUNT = 0x20000;

        std::unique_ptr<audio_input_stream> stream_;
        drivers::audio_driver *aud_;

        common::ring_buffer<std::uint16_t, RING_BUFFER_MAX_SAMPLE_COUNT> ring_buffer_;
        std::mutex callback_lock_;

        std::queue<input_read_request> read_queue_;
        std::uint32_t read_bytes_; 

    protected:
        std::size_t record_data_callback(std::int16_t *buffer, std::size_t frames);

    public:
        explicit dsp_input_stream_shared(drivers::audio_driver *aud);
        virtual ~dsp_input_stream_shared() override;

        bool read(std::uint8_t *data, const std::uint32_t max_data_size) override;

        bool set_properties(const std::uint32_t freq, const std::uint8_t channels) override;

        virtual bool start() override;
        virtual bool stop() override;

        virtual std::uint64_t real_time_position() override;
        std::uint64_t position() override;
        
        bool is_recording() const override {
            return stream_->is_recording();
        }
 
        virtual void get_supported_formats(std::vector<four_cc> &cc_list) override {
            cc_list.clear();
        }
    };
}