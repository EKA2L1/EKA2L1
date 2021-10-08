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
#include <drivers/audio/backend/dsp_shared.h>

namespace eka2l1::drivers {
    dsp_output_stream_shared::dsp_output_stream_shared(drivers::audio_driver *aud)
        : dsp_output_stream()
        , aud_(aud)
        , pointer_(0)
        , virtual_stop(true)
        , more_requested(false)
        , avg_frame_count_(0) {
        last_frame_[0] = 0;
        last_frame_[1] = 0;
    }

    dsp_output_stream_shared::~dsp_output_stream_shared() {
        if (stream_) {
            stream_->stop();
        }
    }

    bool dsp_output_stream_shared::set_properties(const std::uint32_t freq, const std::uint8_t channels) {
        // Doc said: Writing to the stream must have stopped before you call this function.
        if ((channels_ == channels) && (freq_ == freq)) {
            return true;
        }

        if ((channels == 0) || (freq == 0)) {
            return true;
        }

        bool was_already_stopped = virtual_stop;

        if (stream_) {
            stream_->stop();
            stream_.reset();
        }

        channels_ = channels;
        freq_ = freq;

        stream_ = aud_->new_output_stream(freq, channels, [this](std::int16_t *buffer, const std::size_t nb_frames) {
            return data_callback(buffer, nb_frames);
        });

        if (stream_)
            stream_->set_volume(static_cast<float>(volume_) / 100.0f);

        if (!was_already_stopped) {
            stream_->start();
        }

        return true;
    }

    void dsp_output_stream_shared::volume(const std::uint32_t new_volume) {
        dsp_output_stream::volume(new_volume);

        if (!stream_) {
            return;
        }

        stream_->set_volume(static_cast<float>(new_volume) / 100.0f);
    }

    bool dsp_output_stream_shared::start() {
        if (!stream_) {
            channels_ = 1;
            freq_ = 8000;

            // Create default stream. This follows default MMFDevSound default setting closely
            // Even though this is a generic stream... ;)
            stream_ = aud_->new_output_stream(8000, 1, [this](std::int16_t *buffer, const std::size_t nb_frames) {
                return data_callback(buffer, nb_frames);
            });

            virtual_stop = true;
        }

        avg_frame_count_ = 0;

        if (virtual_stop) {
            if (!stream_->start()) {
                return false;
            }

            virtual_stop = false;
        }

        return true;
    }

    bool dsp_output_stream_shared::stop() {
        if (!stream_)
            return true;

        const std::lock_guard<std::mutex> guard(callback_lock_);

        // Call the finish callback
        if (complete_callback_)
            complete_callback_(complete_userdata_);

        virtual_stop = true;

        // Discard all buffers
        while (auto buffer = buffers_.pop()) {
        }

        return true;
    }

    void dsp_output_stream_shared::register_callback(dsp_stream_notification_type nof_type, dsp_stream_notification_callback callback,
        void *userdata) {
        const std::lock_guard<std::mutex> guard(callback_lock_);

        switch (nof_type) {
        case dsp_stream_notification_done:
            complete_callback_ = callback;
            complete_userdata_ = userdata;
            break;

        case dsp_stream_notification_more_buffer:
            more_buffer_userdata_ = userdata;
            more_buffer_callback_ = callback;
            break;

        default:
            LOG_ERROR(DRIVER_AUD, "Unsupport notification type!");
            break;
        }
    }

    void *dsp_output_stream_shared::get_userdata(dsp_stream_notification_type nof_type) {
        switch (nof_type) {
        case dsp_stream_notification_done:
            return complete_userdata_;

        case dsp_stream_notification_more_buffer:
            return more_buffer_userdata_;

        default:
            LOG_ERROR(DRIVER_AUD, "Unsupport notification type!");
            break;
        }

        return nullptr;
    }

    bool dsp_output_stream_shared::write(const std::uint8_t *data, const std::uint32_t data_size) {
        // Copy buffer to queue
        dsp_buffer buffer;
        buffer.resize(data_size);

        std::memcpy(&buffer[0], data, data_size);

        // Push it to the queue
        buffers_.push(buffer);

        return true;
    }

    bool dsp_output_stream_shared::internal_decode_running_out() {
        if (format_ == PCM16_FOUR_CC_CODE) {
            return (decoded_.size() - pointer_) <= (avg_frame_count_ * channels_ * sizeof(std::int16_t) * 4);
        }

        return false;
    }

    std::size_t dsp_output_stream_shared::data_callback(std::int16_t *buffer, const std::size_t frame_count) {
        std::size_t frame_wrote = 0;

        if (avg_frame_count_ == 0) {
            avg_frame_count_ = frame_count;
        } else {
            avg_frame_count_ = (avg_frame_count_ + frame_count) / 2;
        }

        while (frame_wrote < frame_count) {
            std::optional<dsp_buffer> encoded;

            bool exit_loop = false;
            bool need_more = false;

            if ((decoded_.size() == 0) || (decoded_.size() == pointer_)) {
                // Get the next buffer
                encoded = buffers_.pop();

                if ((format_ == PCM16_FOUR_CC_CODE) && !encoded) {
                    exit_loop = true;
                    break;
                }

                pointer_ = 0;

                if (format_ == PCM16_FOUR_CC_CODE) {
                    decoded_ = encoded.value();
                } else {
                    std::vector<std::uint8_t> empty;
                    decode_data(encoded ? encoded.value() : empty, decoded_);
                }

                if (decoded_.empty()) {
                    exit_loop = true;
                }

                if (!exit_loop) {
                    more_requested = false;
                }

                samples_copied_ += decoded_.size() / sizeof(std::uint16_t);
            }

            if (exit_loop) {
                break;
            }

            // We want to decode more
            std::size_t frame_to_wrote = ((decoded_.size() - pointer_) / channels_ / sizeof(std::int16_t));
            frame_to_wrote = std::min<std::size_t>(frame_to_wrote, frame_count - frame_wrote);

            std::size_t sample_to_wrote = frame_to_wrote * channels_;
            std::size_t size_to_wrote = frame_to_wrote * channels_ * sizeof(std::int16_t);

            // If the amount of buffer left is deemed to be insufficient (this takes account of current frame count that is needed)
            if (internal_decode_running_out()) {
                if (!more_requested && (buffers_.size() == 0)) {
                    const std::lock_guard<std::mutex> guard(callback_lock_);
                    if (more_buffer_callback_) {
                        more_buffer_callback_(more_buffer_userdata_);
                    }
                    
                    more_requested = true;
                }
            }
            
            std::memcpy(&buffer[frame_wrote * channels_], &decoded_[pointer_], size_to_wrote);

            // Set last frame
            std::memcpy(last_frame_, &decoded_[pointer_ + (frame_to_wrote - 1) * channels_ * sizeof(std::int16_t)], channels_ * sizeof(std::int16_t));

            pointer_ += size_to_wrote;
            samples_played_ += sample_to_wrote;

            frame_wrote += frame_to_wrote;
        }

        // We dont want to drain the audio driver, so fill it with last frame
        if (frame_wrote < frame_count) {
            std::memset(&buffer[frame_wrote * channels_], 0, (frame_count - frame_wrote) * channels_ * sizeof(std::int16_t));            
        }

        return frame_count;
    }

    std::uint64_t dsp_output_stream_shared::position() {
        return samples_played_ * 1000000ULL / freq_;
    }

    std::uint64_t dsp_output_stream_shared::real_time_position() {
        std::uint64_t frame_streamed = 0;
        if (!stream_->current_frame_position(&frame_streamed)) {
            LOG_ERROR(DRIVER_AUD, "Fail to retrieve streamed sample count!");
            return 0;
        }

        return frame_streamed * channels_ * 1000000ULL / freq_;
    }
}