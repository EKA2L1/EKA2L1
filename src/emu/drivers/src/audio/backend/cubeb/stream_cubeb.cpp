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

#include <drivers/audio/backend/cubeb/stream_cubeb.h>
#include <drivers/audio/audio.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/platform.h>

namespace eka2l1::drivers {
    static long data_callback_redirector(cubeb_stream *stm, void *user,
        const void *input_buffer, void *output_buffer, long nframes) {
        cubeb_audio_output_stream *stream = reinterpret_cast<cubeb_audio_output_stream *>(user);
        return static_cast<long>(stream->call_callback(reinterpret_cast<std::int16_t *>(output_buffer), nframes));
    }

    void state_callback_redirector(cubeb_stream *stream, void *user_data, cubeb_state state) {
    }

    cubeb_audio_output_stream::cubeb_audio_output_stream(audio_driver *driver, cubeb *context, const std::uint32_t sample_rate,
        const std::uint8_t channels, data_callback callback)
        : audio_output_stream(driver, sample_rate, channels)
        , stream_(nullptr)
        , callback_(callback)
        , playing_(false)
        , pausing_(false)
        , volume_(1.0f)
        , idled_frames_(0) {
        cubeb_stream_params params;
        params.format = CUBEB_SAMPLE_S16LE;
        params.rate = sample_rate;
        params.channels = channels;
        params.layout = (channels == 1) ? CUBEB_LAYOUT_MONO : CUBEB_LAYOUT_STEREO;
        params.prefs = CUBEB_STREAM_PREF_NONE;

        std::uint32_t minimum_latency;
#ifdef EKA2L1_PLATFORM_ANDROID
        minimum_latency = 256;
#else
        minimum_latency = 100 * sample_rate / 1000; // Firefox default

        if (cubeb_get_min_latency(context, &params, &minimum_latency) != CUBEB_OK) {
            LOG_ERROR(DRIVER_AUD, "Error trying to get minimum latency. Use default");
        }
#endif

#ifdef EKA2L1_PLATFORM_ANDROID
        minimum_latency = common::max<std::uint32_t>(256U, minimum_latency);
#endif

        const auto result = cubeb_stream_init(context, &stream_, "EKA2L1 StreamPeam",
            nullptr, nullptr, nullptr, &params, minimum_latency,
            data_callback_redirector, state_callback_redirector, this);

        if (result != CUBEB_OK) {
            LOG_CRITICAL(DRIVER_AUD, "Error trying to initialize cubeb stream!");
            return;
        }
    }

    cubeb_audio_output_stream::~cubeb_audio_output_stream() {
        pausing_ = false;

        if (stream_) {
            cubeb_stream_destroy(stream_);
        }
    }

    std::size_t cubeb_audio_output_stream::call_callback(std::int16_t *output_buffer, const long frames) {
        if (pausing_ || driver_->suspending()) {
            std::memset(output_buffer, 0, frames * channels * sizeof(std::int16_t));
            idled_frames_ += static_cast<std::uint64_t>(frames);
  
            return static_cast<std::size_t>(frames);
        }

        return callback_(output_buffer, frames);
    }

    bool cubeb_audio_output_stream::start() {
        if (pausing_) {
            pausing_ = false;
            return true;
        }

        if (playing_) {
            return true;
        }

        if (cubeb_stream_start(stream_) == CUBEB_OK) {
            playing_ = true;
            idled_frames_ = 0;

            return true;
        }

        return false;
    }

    bool cubeb_audio_output_stream::stop() {
        if (!playing_) {
            return true;
        }

        pausing_ = false;

        if (cubeb_stream_stop(stream_) == CUBEB_OK) {
            playing_ = false;
            return true;
        }

        return false;
    }

    void cubeb_audio_output_stream::pause() {
        pausing_ = true;
    }

    bool cubeb_audio_output_stream::is_playing() {
        return playing_;
    }

    bool cubeb_audio_output_stream::is_pausing() {
        return pausing_;
    }

    bool cubeb_audio_output_stream::set_volume(const float volume) {
        if (cubeb_stream_set_volume(stream_, volume * static_cast<float>(driver_->master_volume() / 100.0f)) == CUBEB_OK) {
            volume_ = volume;
            return true;
        }

        return false;
    }

    float cubeb_audio_output_stream::get_volume() const {
        return volume_;
    }

    bool cubeb_audio_output_stream::current_frame_position(std::uint64_t *pos)  {
        if (cubeb_stream_get_position(stream_, pos) != CUBEB_OK) {
            return false;
        }

        if (idled_frames_ >= *pos) {
            *pos = 0;
        } else {
            *pos -= idled_frames_;
        }

        return true;
    }
}
