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

#include <drivers/audio/audio.h>
#include <drivers/audio/backend/cubeb/audio_cubeb.h>

namespace eka2l1::drivers {
    audio_driver::audio_driver(const std::uint32_t initial_master_volume)
        : master_volume_(initial_master_volume) {
    }

    std::size_t audio_driver::add_master_volume_change_callback(master_audio_volume_change_callback callback) {
        const std::lock_guard<std::mutex> guard(lock_);
        return master_volume_change_callbacks_.add(callback);
    }

    bool audio_driver::remove_master_volume_change_callback(const std::size_t handle) {
        const std::lock_guard<std::mutex> guard(lock_);
        return master_volume_change_callbacks_.remove(handle);
    }

    void audio_driver::master_volume(const std::uint32_t value) {
        const std::lock_guard<std::mutex> guard(lock_);
        const std::uint32_t oldval = master_volume_;

        if (value != oldval) {
            master_volume_ = value;
            for (auto &callback: master_volume_change_callbacks_) {
                if (callback) {
                    callback(oldval, master_volume_);
                }
            }
        }
    }

    audio_driver_instance make_audio_driver(const audio_driver_backend backend, const std::uint32_t initial_master_vol) {
        switch (backend) {
        case audio_driver_backend::cubeb: {
            return std::make_unique<cubeb_audio_driver>(initial_master_vol);
        }

        default:
            break;
        }

        return nullptr;
    }
}
