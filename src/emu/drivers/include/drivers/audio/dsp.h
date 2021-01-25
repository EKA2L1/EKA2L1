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

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace eka2l1::drivers {
    class audio_driver;

    using four_cc = std::uint32_t;

    constexpr four_cc make_four_cc(const char c1, const char c2, const char c3, const char c4) {
        return ((c1 << 24) | (c2 << 16) | (c3 << 8) | c4);
    }

    static constexpr four_cc AMR_FOUR_CC_CODE = make_four_cc(' ', 'A', 'M', 'R');
    static constexpr four_cc MP3_FOUR_CC_CODE = make_four_cc(' ', 'M', 'P', '3');
    static constexpr four_cc PCM16_FOUR_CC_CODE = make_four_cc(' ', 'P', '1', '6');
    static constexpr four_cc PCM8_FOUR_CC_CODE = make_four_cc(' ', ' ', 'P', '8');

    enum dsp_stream_notification_type {
        dsp_stream_notification_buffer_copied = 0,
        dsp_stream_notification_done = 1
    };

    using dsp_stream_notification_callback = std::function<void(void *)>;
    using dsp_stream_userdata = void *;

    struct dsp_stream {
    protected:
        std::size_t samples_played_;
        std::size_t samples_copied_;

        std::uint32_t freq_;
        std::uint32_t channels_;

        four_cc format_;

        dsp_stream_notification_callback complete_callback_;
        dsp_stream_notification_callback buffer_copied_callback_;

        dsp_stream_userdata complete_userdata_;
        dsp_stream_userdata buffer_copied_userdata_;

    public:
        explicit dsp_stream();
        virtual ~dsp_stream() = default;
        virtual const std::uint32_t samples_played() const {
            return static_cast<std::uint32_t>(samples_played_);
        }

        virtual const std::uint64_t samples_copied() const {
            return samples_copied_;
        }

        const std::uint64_t bytes_rendered() const {
            return samples_played_ * channels_ * sizeof(std::uint16_t);
        }

        virtual const four_cc format() const {
            return format_;
        }

        virtual bool format(const four_cc fmt) {
            format_ = fmt;
            return true;
        }

        /**
         * @brief       Get the position of the stream, calculated with the buffers received from read/write.
         * @returns     The copied position of the stream.
         */
        virtual std::uint64_t position() = 0;

        /**
         * @brief       Get the position of the stream, as the stream is playing.
         * @returns     The real-time position of the stream.
         */
        virtual std::uint64_t real_time_position() = 0;

        virtual bool set_properties(const std::uint32_t freq, const std::uint8_t channels) = 0;
        virtual void get_supported_formats(std::vector<four_cc> &cc_list) = 0;

        virtual bool start() = 0;
        virtual bool stop() = 0;

        virtual void register_callback(dsp_stream_notification_type nof_type, dsp_stream_notification_callback callback,
            void *userdata)
            = 0;

        virtual void *get_userdata(dsp_stream_notification_type nof_type) = 0;
        virtual void reset_stat();

        virtual bool is_playing() const = 0;
        virtual bool is_recording() const = 0;
    };

    struct dsp_output_stream : public dsp_stream {
    protected:
        std::uint32_t volume_;

    public:
        explicit dsp_output_stream();

        virtual ~dsp_output_stream() = default;
        virtual bool write(const std::uint8_t *data, const std::uint32_t data_size) = 0;

        virtual const std::uint32_t volume() const {
            return volume_;
        }

        virtual void volume(const std::uint32_t new_volume) {
            volume_ = new_volume;
        }

        virtual std::uint32_t max_volume() const {
            return 100;
        }

        virtual bool is_playing() const override {
            return false;
        }

        virtual bool is_recording() const override {
            return false;
        }
    };

    enum dsp_stream_backend {
        dsp_stream_backend_ffmpeg = 0
    };

    std::unique_ptr<dsp_stream> new_dsp_out_stream(drivers::audio_driver *aud, const dsp_stream_backend dsp_backend);
}
