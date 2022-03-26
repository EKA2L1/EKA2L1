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
#include <drivers/audio/backend/player_shared.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>

namespace eka2l1::drivers {
    std::size_t player_shared::data_supply_callback(std::int16_t *data, std::size_t size) {
        // Get the oldest request
        const std::lock_guard<std::mutex> guard(lock_);

        std::size_t frame_copied = 0;

        auto supply_stuff = [&]() {
            while ((frame_copied < size) && (!(flags_ & 1))) {
                if (data_.size() <= data_pointer_)
                    get_more_data();

                const std::size_t total_frame_left = (data_.size() - data_pointer_ + 1) / channels_ / sizeof(std::uint16_t);
                const std::size_t frame_to_copy = std::min<std::size_t>(total_frame_left, size - frame_copied);

                std::memcpy(data + frame_copied * channels_, data_.data() + data_pointer_, frame_to_copy * channels_ * sizeof(std::uint16_t));

                data_pointer_ += frame_to_copy * channels_ * sizeof(std::uint16_t);
                frame_copied += frame_to_copy;
            }
        };

        supply_stuff();

        if ((frame_copied < size) || (flags_ & 1)) {
            bool no_more_way = false;

            // There is no more data for us! Either repeat or kill
            if (repeat_left_ == 0) {
                no_more_way = true;
            } else {
                // Seek back to do a loop. Intentionally left this so that negative repeat can do infinite loop
                if (repeat_left_ > 0) {
                    repeat_left_ -= 1;
                }

                // Reset the stream if we are the custom format guy!
                reset_request();

                data_pointer_ = 0;
                flags_ = 0;

                // We want to supply silence samples
                // 1s = 1ms
                const std::size_t silence_samples = freq_ * silence_micros_ / 1000000;
                data_.resize(silence_samples * sizeof(std::uint16_t) * channels_);
                std::fill(data_.begin(), data_.end(), 0);

                use_push_new_data_ = true;
                supply_stuff();
            }

            // We are drained (out of frame)
            // Call the finish callback
            if (no_more_way && callback_) {
                callback_(userdata_.data());
            }
        }

        return frame_copied;
    }

    bool player_shared::play() {
        // Stop previous session
        if (output_stream_) {
            if (output_stream_->is_pausing()) {
                output_stream_->start();
                return true;
            }

            output_stream_->stop();
        }

        // Reset the request
        {
            const std::lock_guard<std::mutex> guard(lock_);
            reset_request();

            if (!is_ready_to_play()) {
                return false;
            }

            data_pointer_ = 0;
            flags_ = 0;
            data_.clear();

            // New stream to restart everything
            output_stream_ = aud_->new_output_stream(freq_, channels_, [this](std::int16_t *u1, std::size_t u2) {
                return data_supply_callback(u1, u2);
            });

            if (!output_stream_) {
                // Null stream
                return true;
            }

            output_stream_->set_volume(static_cast<float>(volume_) / 10.0f);
        }

        return output_stream_->start();
    }

    bool player_shared::stop() {
        if (output_stream_)
            return output_stream_->stop();

        return true;
    }

    void player_shared::pause() {
        if (output_stream_) {
            output_stream_->pause();
        }
    }

    bool player_shared::notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size) {
        return player::notify_any_done(callback, data, data_size);
    }

    void player_shared::clear_notify_done() {
        return player::clear_notify_done();
    }

    void player_shared::set_repeat(const std::int32_t repeat_times, const std::int64_t silence_intervals_micros) {
        repeat_left_ = repeat_times;
        silence_micros_ = silence_intervals_micros;

        if (silence_micros_ < 0) {
            silence_micros_ = 0;
        }
    }

    void player_shared::set_position(const std::uint64_t pos_in_us) {
        const std::lock_guard<std::mutex> guard(lock_);
        set_position_for_custom_format(pos_in_us);
    }

    bool player_shared::set_dest_freq(const std::uint32_t freq) {
        freq_ = freq;
        return true;
    }

    bool player_shared::set_dest_channel_count(const std::uint32_t cn) {
        channels_ = cn;
        return true;
    }

    bool player_shared::set_dest_encoding(const std::uint32_t enc) {
        encoding_ = enc;
        return true;
    }

    void player_shared::set_dest_container_format(const std::uint32_t confor) {
        format_ = confor;
    }

    bool player_shared::set_volume(const std::uint32_t vol) {
        const std::lock_guard<std::mutex> guard(lock_);
        const bool res = player::set_volume(vol);

        if (output_stream_ && res) {
            output_stream_->set_volume(static_cast<float>(volume_) / static_cast<float>(max_volume()));
        }

        return res;
    }

    std::uint32_t player_shared::get_dest_freq() {
        return freq_;
    }

    std::uint32_t player_shared::get_dest_channel_count() {
        return channels_;
    }

    std::uint32_t player_shared::get_dest_encoding() {
        return encoding_;
    }

    std::uint64_t player_shared::position() const {
        std::uint64_t pos_in_frames = 0;
        if (!output_stream_ || !freq_ || !output_stream_->current_frame_position(&pos_in_frames)) {
            return 0;
        }
        return pos_in_frames * channels_ * 1000000ULL / freq_;
    }

    player_shared::player_shared(audio_driver *driver)
        : aud_(driver)
        , data_pointer_(0)
        , flags_(0)
        , repeat_left_(0)
        , silence_micros_(0)
        , pos_in_us_(0)
        , use_push_new_data_(false) {
    }

    player_shared::~player_shared() {
        if (output_stream_) {
            output_stream_->stop();
        }
    }
}