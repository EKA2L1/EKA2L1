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
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        player_request_instance &request = requests_.front();

        std::size_t frame_copied = 0;

        auto supply_stuff = [&]() {
            while ((frame_copied < size) && (!(request->flags_ & 1))) {
                if (request->data_.size() == request->data_pointer_)
                    get_more_data(request);

                const std::size_t total_frame_left = (request->data_.size() - request->data_pointer_) / request->channels_ / sizeof(std::uint16_t);
                const std::size_t frame_to_copy = std::min<std::size_t>(total_frame_left, size - frame_copied);

                // Copy the frames first
                if (request->channels_ == 1) {
                    const std::uint16_t *original_data_u16 = reinterpret_cast<std::uint16_t *>(request->data_.data() + request->data_pointer_);

                    // We have to do something!
                    for (std::size_t frame_ite = 0; frame_ite < frame_to_copy; frame_ite++) {
                        data[2 * (frame_ite + frame_copied)] = original_data_u16[frame_ite];
                        data[2 * (frame_ite + frame_copied) + 1] = original_data_u16[frame_ite];
                    }
                } else {
                    // Well, just copy straight up
                    std::memcpy(data + frame_copied * 2, request->data_.data() + request->data_pointer_, frame_to_copy * 2 * sizeof(std::uint16_t));
                }

                request->data_pointer_ += frame_to_copy * request->channels_ * sizeof(std::uint16_t);
                frame_copied += frame_to_copy;
            }
        };

        supply_stuff();

        if ((frame_copied < size) || (request->flags_ & 1)) {
            bool no_more_way = false;

            // There is no more data for us! Either repeat or kill
            if (request->repeat_left_ == 0) {
                no_more_way = true;
            } else {
                // Seek back to do a loop. Intentionally left this so that negative repeat can do infinite loop
                if (request->repeat_left_ > 0) {
                    request->repeat_left_ -= 1;
                }

                // Reset the stream if we are the custom format guy!
                if (request->type_ == player_request_play_format) {
                    reset_request(request);
                }

                request->data_pointer_ = 0;
                request->flags_ = 0;

                // We want to supply silence samples
                // 1s = 1ms
                const std::size_t silence_samples = request->freq_ * request->silence_micros_ / 1000000;
                request->data_.resize(silence_samples * sizeof(std::uint16_t) * request->channels_);
                std::fill(request->data_.begin(), request->data_.end(), 0);

                request->use_push_new_data_ = true;
                supply_stuff();
            }

            // We are drained (out of frame)
            // Call the finish callback
            if (no_more_way && callback_)
                callback_(userdata_.data());
        }

        return frame_copied;
    }

    bool player_shared::play() {
        // Stop previous session
        if (output_stream_)
            output_stream_->stop();

        // Reset the request
        {
            const std::lock_guard<std::mutex> guard(request_queue_lock_);
            player_request_instance &request = requests_.front();

            reset_request(request);

            request->data_pointer_ = 0;
            request->flags_ = 0;
            request->data_.clear();

            // New stream to restart everything
            output_stream_ = aud_->new_output_stream(request->freq_, 2, [this](std::int16_t *u1, std::size_t u2) {
                return data_supply_callback(u1, u2);
            });

            if (!output_stream_) {
                // Null stream
                return true;
            }

            output_stream_->set_volume(static_cast<float>(volume_) / 100.0f);
        }

        return output_stream_->start();
    }

    bool player_shared::stop() {
        if (output_stream_)
            return output_stream_->stop();

        return true;
    }

    bool player_shared::notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size) {
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        return player::notify_any_done(callback, data, data_size);
    }

    void player_shared::clear_notify_done() {
        std::unique_lock<std::mutex> guard(request_queue_lock_, std::try_to_lock);
        return player::clear_notify_done();
    }

    void player_shared::set_repeat(const std::int32_t repeat_times, const std::uint64_t silence_intervals_micros) {
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        player_request_instance &request = requests_.front();

        request->repeat_left_ = repeat_times;
        request->silence_micros_ = silence_intervals_micros;
    }

    void player_shared::set_position(const std::uint64_t pos_in_us) {
        player_request_instance &request_ref = requests_.back();

        if (!request_ref) {
            return;
        }

        switch (request_ref->type_) {
        case player_request_play_format:
        case player_request_record_raw_format:
        case player_request_crop: {
            set_position_for_custom_format(request_ref, pos_in_us);
            break;
        }

        case player_request_play_raw_pcm: {
            // Calculate the data position by using audio hz
            // Hz / 1000000 = frames per microseconds
            std::uint64_t total_frame_to_seek_to = common::multiply_and_divide_qwords(pos_in_us, request_ref->freq_, 1000000);
            request_ref->data_pointer_ = total_frame_to_seek_to * sizeof(std::uint16_t) * request_ref->channels_;

            if (request_ref->data_pointer_ >= request_ref->data_.size()) {
                LOG_TRACE("Setting position for PCM overflowed! Resetting the position to 0");
                request_ref->data_pointer_ = 0;
            }

            break;
        }

        default:
            break;
        }
    }

    player_shared::player_shared(audio_driver *driver)
        : aud_(driver) {
    }

    player_shared::~player_shared() {
        if (output_stream_) {
            output_stream_->stop();
        }
    }
}