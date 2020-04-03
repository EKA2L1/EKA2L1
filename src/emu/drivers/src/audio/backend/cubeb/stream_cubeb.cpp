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

#include <common/algorithm.h>
#include <common/log.h>

namespace eka2l1::drivers {
    static long data_callback_redirector(cubeb_stream * stm, void * user,
        const void * input_buffer, void * output_buffer, long nframes) {
        cubeb_audio_output_stream *stream = reinterpret_cast<cubeb_audio_output_stream*>(user);
        return static_cast<long>(stream->call_callback(reinterpret_cast<std::int16_t*>(output_buffer), nframes));
    }

    void state_callback_redirector(cubeb_stream* stream, void* user_data, cubeb_state state) {

    }

    cubeb_audio_output_stream::cubeb_audio_output_stream(cubeb *context_, const std::uint32_t sample_rate,
        data_callback callback) 
        : stream_(nullptr)
        , callback_(callback)
        , playing_(false) {
        cubeb_stream_params params;
        params.format = CUBEB_SAMPLE_S16LE;
        params.rate = sample_rate;
        params.channels = 2;
        params.layout = CUBEB_LAYOUT_STEREO;
        params.prefs = CUBEB_STREAM_PREF_NONE;

        std::uint32_t minimum_latency = 100 * sample_rate / 1000; // Firefox default
        if (cubeb_get_min_latency(context_, &params, &minimum_latency) != CUBEB_OK) {
            LOG_ERROR("Error trying to get minimum latency. Use default");
        }
        
        const auto result = cubeb_stream_init(context_, &stream_, "EKA2L1 StreamPeam",
            nullptr, nullptr, nullptr, &params, common::max<std::uint32_t>(512U, minimum_latency),
            data_callback_redirector, state_callback_redirector, this);

        if (result != CUBEB_OK) {
            LOG_CRITICAL("Error trying to initialize cubeb stream!");
            return;
        }
    }

    cubeb_audio_output_stream::~cubeb_audio_output_stream() {
        if (stream_) {
            cubeb_stream_destroy(stream_);
        }
    }

    std::size_t cubeb_audio_output_stream::call_callback(std::int16_t *output_buffer, const long frames) {
        return callback_(output_buffer, frames);
    }

    bool cubeb_audio_output_stream::start() {
        if (playing_) {
            return true;
        }

        if (cubeb_stream_start(stream_) == CUBEB_OK) {
            playing_ = true;
            return true;
        }

        return false;
    }
    
    bool cubeb_audio_output_stream::stop() {
        if (cubeb_stream_stop(stream_) == CUBEB_OK) {
            playing_ = false;
            return true;
        }

        return false;
    }
    
    bool cubeb_audio_output_stream::is_playing() {
        return playing_;
    }

    bool cubeb_audio_output_stream::set_volume(const float volume) {
        if (cubeb_stream_set_volume(stream_, volume) == CUBEB_OK) {
            return true;
        }

        return false;
    }
}