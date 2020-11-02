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

#include <common/log.h>
#include <common/platform.h>
#include <drivers/audio/backend/cubeb/audio_cubeb.h>
#include <drivers/audio/backend/cubeb/stream_cubeb.h>

#if EKA2L1_PLATFORM(WIN32)
#include <objbase.h>
#endif

namespace eka2l1::drivers {
    cubeb_audio_driver::cubeb_audio_driver()
        : context_(nullptr)
        , init_(false) {
        if (cubeb_init(&context_, "EKA2L1 Audio Driver", nullptr) != CUBEB_OK) {
            LOG_CRITICAL("Can't initialize Cubeb audio driver!");
            return;
        }

        init_ = true;
    }

    cubeb_audio_driver::~cubeb_audio_driver() {
        if (context_) {
            cubeb_destroy(context_);
        }
    }

    std::uint32_t cubeb_audio_driver::native_sample_rate() {
        std::uint32_t preferred_rate = 0;

#ifdef EKA2L1_PLATFORM_ANDROID
        preferred_rate = 48000;
#else
        const auto result = cubeb_get_preferred_sample_rate(context_, &preferred_rate);

        if (result != CUBEB_OK) {
            return 0;
        }
#endif

        return preferred_rate;
    }

    std::unique_ptr<audio_output_stream> cubeb_audio_driver::new_output_stream(const std::uint32_t sample_rate,
        const std::uint8_t channels, data_callback callback) {
        if (!init_) {
            return nullptr;
        }

        return std::make_unique<cubeb_audio_output_stream>(context_, sample_rate, channels, callback);
    }
};