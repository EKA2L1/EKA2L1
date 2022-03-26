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

#include <drivers/audio/backend/minibae/player_minibae.h>
#include <drivers/audio/backend/baeplat_impl.h>
#include <drivers/audio/audio.h>

#include <common/log.h>
#include <common/buffer.h>

#include <memory>

namespace eka2l1::drivers {
    struct BAEMixerWrapper {
    private:
        BAEMixer mixer_;
        BAEBankToken current_bank_;

        std::size_t bank_change_callback_handle_;

    public:
        explicit BAEMixerWrapper()
            : mixer_(nullptr)
            , current_bank_(nullptr)
            , bank_change_callback_handle_(0) {
        }

        ~BAEMixerWrapper() {
            if (mixer_) {
                BAEMixer_Close(mixer_);
            }

            if (bank_change_callback_handle_) {
                audio_driver *drv = BAE_GetActiveAudioDriver();
                if (drv) {
                    drv->remove_bank_change_callback(bank_change_callback_handle_);
                }
            }
        }

        void handle_bank_change(const midi_bank_type type, const std::string &new_path) {
            if (type == MIDI_BANK_TYPE_HSB) {
                BAEBankToken new_token = nullptr;
                BAEResult result = BAEMixer_AddBankFromFile(mixer_, const_cast<char*>(new_path.data()), &new_token);

                if (result != BAE_NO_ERROR) {
                    LOG_ERROR(DRIVER_AUD, "Failed to new miniBae banks!");
                } else {
                    BAEMixer_UnloadBank(mixer_, current_bank_);
                    current_bank_ = new_token;
                }
            }
        }

        BAEMixer get_mixer() {
            if (!mixer_) {
                mixer_ = BAEMixer_New();
                if (!mixer_) {
                    return nullptr;
                }

                BAEResult result = BAEMixer_Open(mixer_, BAE_RATE_44K, BAE_LINEAR_INTERPOLATION,
                    BAE_USE_16 | BAE_USE_STEREO | BAE_DISABLE_REVERB, BAE_MAX_VOICES, 0, BAE_MAX_VOICES / 2, 1);

                if (result != BAE_NO_ERROR) {
                    LOG_ERROR(DRIVER_AUD, "Failed to open miniBae mixer!");
                    return nullptr;
                }

                std::string default_bank_path("resources/defaultbank.hsb");
                audio_driver *drv = BAE_GetActiveAudioDriver();

                if (drv) {
                    std::optional<std::string> path = drv->get_bank_path(MIDI_BANK_TYPE_HSB);
                    if (path.has_value()) {
                        default_bank_path = path.value();
                    }
                }

                result = BAEMixer_AddBankFromFile(mixer_, &default_bank_path[0], &current_bank_);
                if (result != BAE_NO_ERROR) {
                    LOG_ERROR(DRIVER_AUD, "Failed to load miniBae banks!");
                    return nullptr;
                }

                bank_change_callback_handle_ = drv->add_bank_change_callback([&](const midi_bank_type type, const std::string &path) {
                    return this->handle_bank_change(type, path);
                });
            }

            return mixer_;
        }
    };

    BAEMixerWrapper global_minibae_mixer;

    player_minibae::player_minibae(audio_driver *driver)
        : song_(nullptr)
        , repeat_times_(0)
        , paused_(false) {
        BAE_SetActiveAudioDriver(driver);
    }
    
    player_minibae::~player_minibae() {
        paused_ = false;

        if (song_) {
            BAESong_Delete(song_);
        }
    }

    void player_minibae::call_song_done() {
        if (callback_)
            callback_(userdata_.data());
    }
    
    bool player_minibae::play() {
        if (!song_) {
            return false;
        }

        if (paused_) {
            BAESong_Resume(song_);
            paused_ = false;

            return true;
        }

        // Previous stop of same song may got the fade to activate. Reset fade
        BAESong_Fade(song_, FLOAT_TO_FIXED(volume_ / 10.0f), FLOAT_TO_FIXED(volume_ / 10.0f), 0);
        const BAEResult error = BAESong_Start(song_, 0);

        if (error == BAE_NO_ERROR) {
            return true;
        }

        LOG_ERROR(DRIVER_AUD, "Encounter error trying to play MIDI: {}", static_cast<int>(error));
        return false;
    }

    bool player_minibae::record() {
        return false;        
    }

    bool player_minibae::crop() {
        return false;        
    }

    bool player_minibae::stop() {
        if (!song_) {
            return true;
        }

        const BAEResult error = BAESong_Stop(song_, 1);
        
        if (error != BAE_NO_ERROR) {
            LOG_ERROR(DRIVER_AUD, "Encounter error trying to stop MIDI: {}", static_cast<int>(error));
            return false;
        }

        return true;
    }

    void player_minibae::pause() {
        if (!paused_ && song_) {
            BAESong_Pause(song_);
            paused_ = true;
        }
    }
    
    bool player_minibae::set_volume(const std::uint32_t vol) {
        if (!player::set_volume(vol)) {
            return false;
        }

        if (song_) {
            const BAEResult error = BAESong_SetVolume(song_, FLOAT_TO_UNSIGNED_FIXED(vol / 10.0f));

            if (error != BAE_NO_ERROR) {
                LOG_ERROR(DRIVER_AUD, "Encounter error trying to set MIDI song volume: {}", static_cast<int>(error));
                return false;
            }
        }

        return true;
    }

    static void bae_song_done_callback(void *userdata) {
        player_minibae *player = reinterpret_cast<player_minibae*>(userdata);
        player->call_song_done();
    }

    bool player_minibae::open_url(const std::string &url) {
        BAESong new_song = BAESong_New(global_minibae_mixer.get_mixer());
        if (!new_song) {
            return false;
        }

        BAEResult error = BAESong_LoadMidiFromFile(new_song, const_cast<char*>(url.c_str()), true);
    
        if (error != BAE_NO_ERROR) {
            LOG_ERROR(DRIVER_AUD, "Encounter error trying to load MIDI song: {}", static_cast<int>(error));
            BAESong_Delete(new_song);

            return false;
        }

        BAESong_SetCallback(new_song, bae_song_done_callback, this);

        if (song_) {
            BAESong_Delete(song_);
            song_ = nullptr;
        }

        song_ = new_song;
        return true;
    }

    bool player_minibae::open_custom(common::rw_stream *stream) {
        std::vector<std::uint8_t> data_read(stream->size());
        
        std::size_t read = 0;
        std::uint64_t left = stream->size();

        while (left > 0) {
            std::uint64_t to_read = std::min<std::uint64_t>(left, 0x4000ULL);
            std::uint64_t res = stream->read(data_read.data() + read, to_read);
            if (res != to_read) {
                break;
            }
            read += res;
        }

        BAESong new_song = BAESong_New(global_minibae_mixer.get_mixer());
        if (!new_song) {
            return false;
        }

        BAEResult error = BAESong_LoadMidiFromMemory(new_song, data_read.data(), static_cast<unsigned long>(data_read.size()), true);
    
        if (error != BAE_NO_ERROR) {
            LOG_ERROR(DRIVER_AUD, "Encounter error trying to load MIDI song: {}", static_cast<int>(error));
            BAESong_Delete(new_song);

            return false;
        }

        BAESong_SetCallback(new_song, bae_song_done_callback, this);

        if (song_) {
            BAESong_Delete(song_);
            song_ = nullptr;
        }

        song_ = new_song;
        return true;
    }

    bool player_minibae::is_playing() const {
        BAE_BOOL done = 0;
        if (!song_) {
            return false;
        }

        BAESong_IsDone(song_, &done);
        return !done;
    }

    void player_minibae::set_repeat(const std::int32_t repeat_times, const std::int64_t silence_intervals_micros) {
        if (!song_) {
            return;
        }

        // TODO: Ignoring silence intervals here....
        repeat_times_ = repeat_times;

        if (repeat_times_ >= 0) {
            BAESong_SetLoops(song_, repeat_times);
        } else {
            // It does not support infinite loop, so we have to work around with maximum signed short :(
            BAESong_SetLoops(song_, 32767);
        }
    }

    void player_minibae::set_position(const std::uint64_t pos_in_us) {
        if (!song_) {
            return;
        }

        BAESong_SetMicrosecondPosition(song_, static_cast<unsigned long>(pos_in_us));
    }
    
    std::uint64_t player_minibae::position() const {
        if (!song_) {
            return 0;
        }

        unsigned long value = 0;
        BAESong_GetMicrosecondPosition(song_, &value);

        return static_cast<std::uint64_t>(value);
    }

    bool player_minibae::set_dest_encoding(const std::uint32_t enc) {
        return false;
    }

    bool player_minibae::set_dest_freq(const std::uint32_t freq) {
        return false;
    }

    bool player_minibae::set_dest_channel_count(const std::uint32_t cn) {
        return false;
    }

    std::uint32_t player_minibae::get_dest_freq() {
        return 0;
    }

    std::uint32_t player_minibae::get_dest_channel_count() {
        return 0;
    }

    std::uint32_t player_minibae::get_dest_encoding() {
        return 0;
    }

    void player_minibae::set_dest_container_format(const std::uint32_t confor) {
        
    }
}