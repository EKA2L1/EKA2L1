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

/*
 * Copyright (C) 2017-2018 Bernhard Schelling (Based on SFZero, Copyright (C) 2012
 * Steve Folta, https://github.com/stevefolta/SFZero)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

#include <drivers/audio/backend/tinysoundfont/player_tsf.h>
#include <drivers/audio/audio.h>

#include <common/buffer.h>
#include <common/log.h>

#define TSF_IMPLEMENTATION
#define TML_IMPLEMENTATION

#include <TinySoundFont/tsf.h>
#include <TinySoundFont/tml.h>

namespace eka2l1::drivers {
    static constexpr std::uint32_t TSF_SAMPLE_RATE = 44100;
    static constexpr std::uint32_t TSF_CHANNEL_COUNT = 2;

    player_tsf::player_tsf(audio_driver *driver)
        : driver_(driver)
        , output_(nullptr)
        , synth_(nullptr)
        , begin_msg_(nullptr)
        , playing_message_(nullptr)
        , repeat_count_(0)
        , repeat_left_(0)
        , silence_intervals_us_(0)
        , current_msecs_(0)
        , bank_change_callback_(0) {
        output_ = driver->new_output_stream(TSF_SAMPLE_RATE, TSF_CHANNEL_COUNT, [&](std::int16_t *data, const std::size_t fc) {
            return this->data_callback(data, fc);
        });

        std::string bank_path = "resources/defaultbank.sf2";
        std::optional<std::string> bank_path_stored = driver->get_bank_path(drivers::MIDI_BANK_TYPE_SF2);

        if (bank_path_stored.has_value()) {
            bank_path = bank_path_stored.value();
        }

        synth_ = load_bank_from_file(bank_path);

        tsf_channel_set_bank_preset(synth_, 9, 128, 0);
        tsf_set_output(synth_, (TSF_CHANNEL_COUNT == 1) ? TSF_MONO : TSF_STEREO_INTERLEAVED, TSF_SAMPLE_RATE, 0.0f);

        bank_change_callback_ = driver->add_bank_change_callback([&](const midi_bank_type type, const std::string &new_path) {
            return call_bank_change(type, new_path);
        });
    }
    
    player_tsf::~player_tsf() {
        if (synth_) {
            tsf_close(synth_);
        }

        if (begin_msg_) {
            tml_free(begin_msg_);
        }

        if (output_) {
            output_->stop();
        }

        if (driver_) {
            driver_->remove_bank_change_callback(bank_change_callback_);
        }
    }

    std::size_t player_tsf::data_callback(std::int16_t *buffer, const std::size_t frame_count) {
        std::size_t frame_count_org = frame_count;
        std::size_t block_frame_count = TSF_RENDER_EFFECTSAMPLEBLOCK;

        if (!playing_message_) {
            memset(buffer, 0, frame_count * TSF_CHANNEL_COUNT * sizeof(std::int16_t));
            return frame_count;
        }

        while (buffer_queue_.size() < frame_count * TSF_CHANNEL_COUNT) {
            const std::lock_guard<std::mutex> lock(msg_lock_);

            //Loop through all MIDI messages which need to be played up until the current playback time
            for (current_msecs_ += block_frame_count * (1000.0 / TSF_SAMPLE_RATE); playing_message_ && current_msecs_ >= playing_message_->time; playing_message_ = playing_message_->next) {
                switch (playing_message_->type) {
                    case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
                        // This also include a fix by edgarlsitec! Thank you! (Please see issue #59 on Github of TSF)
                        tsf_channel_set_presetnumber(synth_, playing_message_->channel, playing_message_->program, (playing_message_->channel == 9));
                        tsf_channel_midi_control(synth_, playing_message_->channel, TML_ALL_SOUND_OFF, 0);
                        break;
                    case TML_NOTE_ON: //play a note
                        tsf_channel_note_on(synth_, playing_message_->channel, playing_message_->key, playing_message_->velocity / 127.0f);
                        break;
                    case TML_NOTE_OFF: //stop a note
                        tsf_channel_note_off(synth_, playing_message_->channel, playing_message_->key);
                        break;
                    case TML_PITCH_BEND: //pitch wheel modification
                        tsf_channel_set_pitchwheel(synth_, playing_message_->channel, playing_message_->pitch_bend);
                        break;
                    case TML_CONTROL_CHANGE: //MIDI controller messages
                        tsf_channel_midi_control(synth_, playing_message_->channel, playing_message_->control, playing_message_->control_value);
                        break;
                    default:
                        break;
                }
            }

            if (block_buffer.empty()) {
                block_buffer.resize(block_frame_count * TSF_CHANNEL_COUNT);
            }

            // Render the block of audio samples in float format
            tsf_render_short(synth_, block_buffer.data(), static_cast<int>(block_frame_count), 0);
            buffer_queue_.push(block_buffer);
            
            if (!playing_message_) {
                // Generate some silences beforehand
                if (silence_intervals_us_) {
                    std::vector<std::int16_t> silences(silence_intervals_us_ * TSF_SAMPLE_RATE / 1000000 * 2);
                    memset(silences.data(), 0, silences.size() * sizeof(std::int16_t));

                    buffer_queue_.push(silences);
                }

                if (repeat_left_ > 0) {
                    current_msecs_ = 0.0;
                    playing_message_ = begin_msg_;

                    repeat_left_--;
                } else {
                    call_song_done();
                    playing_message_ = nullptr;

                    break;
                }
            }
	    }

        std::vector<std::int16_t> data = buffer_queue_.pop(frame_count * TSF_CHANNEL_COUNT);
        memcpy(buffer, data.data(), data.size() * sizeof(std::int16_t));

        if (data.size() < frame_count * TSF_CHANNEL_COUNT) {
            memset(buffer + data.size(), 0, frame_count * TSF_CHANNEL_COUNT - data.size());
        }

        return frame_count;
    }

    void player_tsf::call_song_done() {
        if (callback_)
            callback_(userdata_.data());
    }

    tsf *player_tsf::load_bank_from_file(const std::string &path) {
        common::ro_std_file_stream stream(path, true);
        if (!stream.valid()) {
            return nullptr;
        }

        tsf_stream tstream;
        tstream.data = &stream;
        tstream.read = [](void *data, void *ptr, unsigned int size) -> int {
            common::ro_stream *me = reinterpret_cast<common::rw_stream*>(data);
            return static_cast<int>(me->read(ptr, size));
        };
        tstream.skip = [](void *data, unsigned int count) -> int {
            common::ro_stream *me = reinterpret_cast<common::rw_stream*>(data);
            me->seek(count, common::seek_where::cur);

            return 1;
        };

        return tsf_load(&tstream);
    }

    void player_tsf::call_bank_change(const midi_bank_type type, const std::string &new_path) {
        if (type == MIDI_BANK_TYPE_SF2) {
            const std::lock_guard<std::mutex> guard(lock_);
            
            if (synth_) {
                tsf *new_synth = load_bank_from_file(new_path);
                if (!new_synth) {
                    LOG_ERROR(DRIVER_AUD, "Fail to load new SF2 bank for TSF!");
                    return;
                }
    
                tsf_close(synth_);
                synth_ = new_synth;
            }
        }
    }
    
    bool player_tsf::play() {
        if (output_ && output_->is_pausing()) {
            output_->start();
            return true;
        }

        if (!begin_msg_) {
            return false;
        }

        if (playing_message_) {
            return true;
        }

        std::lock_guard<std::mutex> guard(msg_lock_);
        playing_message_ = begin_msg_;
        current_msecs_ = 0.0;

        output_->start();
        return true;
    }

    bool player_tsf::record() {
        return false;        
    }

    bool player_tsf::crop() {
        return false;        
    }

    bool player_tsf::stop() {
        if (!begin_msg_) {
            return true;
        }

        std::lock_guard<std::mutex> guard(msg_lock_);
        playing_message_ = nullptr;

        return true;
    }

    void player_tsf::pause() {
        if (output_) {
            output_->pause();
        }
    }
    
    bool player_tsf::set_volume(const std::uint32_t vol) {
        if (!player::set_volume(vol)) {
            return false;
        }

        return output_->set_volume(vol / 10.0f);
    }

    bool player_tsf::open_url(const std::string &url) {
        // Roundabout cause fopen can't read UTF8 url (which is what TML uses)
        common::ro_std_file_stream stream(url, true);
        if (!stream.valid()) {
            return false;
        }

        tsf_stream tstream;
        tstream.data = &stream;
        tstream.read = [](void *data, void *ptr, unsigned int size) -> int {
            common::ro_stream *me = reinterpret_cast<common::rw_stream*>(data);
            return static_cast<int>(me->read(ptr, size));
        };
        tstream.skip = [](void *data, unsigned int count) -> int {
            common::ro_stream *me = reinterpret_cast<common::rw_stream*>(data);
            me->seek(count, common::seek_where::cur);

            return 1;
        };

        tml_message *new_song = tml_load_tsf_stream(&tstream);
        if (!new_song) {
            return false;
        }

        if (playing_message_) {
            output_->stop();
            playing_message_ = nullptr;
        }

        if (begin_msg_) {
            tml_free(begin_msg_);
        }

        begin_msg_ = new_song;
        return true;
    }

    bool player_tsf::open_custom(common::rw_stream *stream) {
        tsf_stream tstream;
        tstream.data = stream;
        tstream.read = [](void *data, void *ptr, unsigned int size) -> int {
            common::rw_stream *me = reinterpret_cast<common::rw_stream*>(data);
            return static_cast<int>(me->read(ptr, size));
        };
        tstream.skip = [](void *data, unsigned int count) -> int {
            common::rw_stream *me = reinterpret_cast<common::rw_stream*>(data);
            me->seek(count, common::seek_where::cur);

            return 1;
        };
    
        tml_message *new_song = tml_load_tsf_stream(&tstream);
        if (!new_song) {
            return false;
        }

        if (playing_message_) {
            output_->stop();
            playing_message_ = nullptr;
        }

        if (begin_msg_) {
            tml_free(begin_msg_);
        }

        begin_msg_ = new_song;
        return true;
    }

    bool player_tsf::is_playing() const {
        return playing_message_;
    }

    void player_tsf::set_repeat(const std::int32_t repeat_times, const std::int64_t silence_intervals_micros) {
        const std::lock_guard<std::mutex> guard(msg_lock_);

        repeat_count_ = repeat_times;
        repeat_left_ = repeat_count_;
        silence_intervals_us_ = silence_intervals_us_;

        if (silence_intervals_us_ < 0) {
            silence_intervals_us_ = 0;
        }
    }

    void player_tsf::set_position(const std::uint64_t pos_in_us) {
        LOG_ERROR(DRIVER_AUD, "Set position is not yet supported with TSF driver!");
    }

    std::uint64_t player_tsf::position() const {
        std::uint64_t pos_in_frames = 0;
        if (!output_ || !output_->current_frame_position(&pos_in_frames)) {
            return 0;
        }
        return pos_in_frames * output_->get_channels() * 1000000ULL / output_->get_sample_rate();
    }

    bool player_tsf::set_dest_encoding(const std::uint32_t enc) {
        return false;
    }

    bool player_tsf::set_dest_freq(const std::uint32_t freq) {
        return false;
    }

    bool player_tsf::set_dest_channel_count(const std::uint32_t cn) {
        return false;
    }

    std::uint32_t player_tsf::get_dest_freq() {
        return 0;
    }

    std::uint32_t player_tsf::get_dest_channel_count() {
        return 0;
    }

    std::uint32_t player_tsf::get_dest_encoding() {
        return 0;
    }

    void player_tsf::set_dest_container_format(const std::uint32_t confor) {
        
    }
}