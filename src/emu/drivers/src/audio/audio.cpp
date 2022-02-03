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

#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)
#include <drivers/audio/backend/wmf/wmf_loader.h>
#endif

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <cstdio>
#include <cstring>

namespace eka2l1::drivers {
    static constexpr const char *MIDI_HEADER_MAGIC = "MThd";
    static constexpr int MIDI_HEADER_MAGIC_LENGTH = 4;

    audio_driver::audio_driver(const std::uint32_t initial_master_volume, const player_type preferred_midi_backend)
        : master_volume_(initial_master_volume)
        , preferred_midi_backend_(preferred_midi_backend) {
    }

    std::vector<player_type> audio_driver::get_suitable_player_types(const std::string &url) {
        std::vector<player_type> res;

        if (!url.empty()) {
            FILE *target_file = common::open_c_file(url, "rb");
    
            if (target_file) {
                char header_magic[MIDI_HEADER_MAGIC_LENGTH];
                if ((fread(header_magic, 1, MIDI_HEADER_MAGIC_LENGTH, target_file) == MIDI_HEADER_MAGIC_LENGTH) &&
                    (strncmp(header_magic, MIDI_HEADER_MAGIC, MIDI_HEADER_MAGIC_LENGTH) == 0)) {
                    res.push_back(preferred_midi_backend_);
                }

                fclose(target_file);
            }
        }

#if EKA2L1_PLATFORM(WIN32)
        if (loader::init_mf()) {
            res.push_back(player_type_wmf);
        } else
#endif  
        {
            res.push_back(player_type_ffmpeg);
        }

        return res;
    }

    void audio_driver::set_preferred_midi_backend(const player_type ttype) {
        if ((ttype != player_type_tsf) && (ttype != player_type_minibae)) {
            return;
        }

        preferred_midi_backend_ = ttype;
    }

    std::optional<std::string> audio_driver::get_bank_path(const midi_bank_type type) const {
        if (type >= MIDI_BANK_TYPE_MAX) {
            return std::nullopt;
        }

        return midi_banks_[static_cast<int>(type)];
    }

    void audio_driver::set_bank_path(const midi_bank_type type, const std::string &path) {
        const std::lock_guard<std::mutex> guard(lock_);

        if (type >= MIDI_BANK_TYPE_MAX) {
            return;
        }

        midi_banks_[static_cast<int>(type)] = path;
        for (auto &cb: bank_change_callbacks_) {
            if (cb) {
                cb(type, path);
            }
        }
    }

    std::size_t audio_driver::add_bank_change_callback(bank_change_callback callback) {
        const std::lock_guard<std::mutex> guard(lock_);
        return bank_change_callbacks_.add(callback);
    }

    bool audio_driver::remove_bank_change_callback(const std::size_t handle) {
        const std::lock_guard<std::mutex> guard(lock_);
        return bank_change_callbacks_.remove(handle);
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

    audio_driver_instance make_audio_driver(const audio_driver_backend backend, const std::uint32_t initial_master_vol,
        const player_type preferred_midi_backend) {
        switch (backend) {
        case audio_driver_backend::cubeb: {
            return std::make_unique<cubeb_audio_driver>(initial_master_vol, preferred_midi_backend);
        }

        default:
            break;
        }

        return nullptr;
    }
}
