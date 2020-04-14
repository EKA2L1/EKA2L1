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

#include <common/queue.h>

#include <vector>
#include <mutex>

namespace eka2l1::drivers {
    using dsp_buffer = std::vector<std::uint8_t>;

    struct dsp_output_stream_shared : public dsp_output_stream {
    protected:
        drivers::audio_driver *aud_;
        std::unique_ptr<drivers::audio_output_stream> stream_;

        std::uint8_t channels_;
        threadsafe_cn_queue<dsp_buffer> buffers_;

        dsp_buffer decoded_;
        std::size_t pointer_;

        std::int16_t last_frame_[2];
        std::mutex callback_lock_;

    public:
        explicit dsp_output_stream_shared(drivers::audio_driver *aud);

        virtual void decode_data(dsp_buffer &original, std::vector<std::uint8_t> &dest) = 0;
        std::size_t data_callback(std::int16_t *buffer, const std::size_t frame_count);

        bool write(const std::uint8_t *data, const std::uint32_t data_size) override;

        virtual void volume(const std::uint32_t new_volume);
        virtual bool set_properties(const std::uint32_t freq, const std::uint8_t channels);

        void register_callback(dsp_stream_notification_type nof_type, dsp_stream_notification_callback callback,
            void *userdata, const std::size_t userdata_size) override;

        virtual bool start();
        virtual bool stop();
    };
}