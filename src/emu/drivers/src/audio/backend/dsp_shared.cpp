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
        , virtual_stop(true)
        , more_requested(false)
        , avg_frame_count_(0) {
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
            stream_->set_volume(static_cast<float>(volume_) / 10.0f);

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

        stream_->set_volume(static_cast<float>(new_volume) / 10.0f);
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

        // Call the finish callback
        if (complete_callback_)
            complete_callback_(complete_userdata_);

        virtual_stop = true;
        more_requested = false;

        buffer_.reset();

        return true;
    }

    bool dsp_output_stream_shared::write(const std::uint8_t *data, const std::uint32_t data_size) {
        // Copy buffer to queue
        if (format_ != PCM16_FOUR_CC_CODE) {
            queue_data_decode(data, data_size);
        } else {
            buffer_.push(data, (data_size + 1) / 2);
        }

        more_requested = false;
        return true;
    }

    bool dsp_output_stream_shared::internal_decode_running_out() {
        if (format_ == PCM16_FOUR_CC_CODE) {
            return (buffer_.size() <= (avg_frame_count_ * channels_ * 4));
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

        if (format_ != PCM16_FOUR_CC_CODE) {
            // Running on low
            if (buffer_.size() <= (avg_frame_count_ * channels_ * 4)) {
                std::vector<std::uint8_t> target_buffer;
                decode_data(target_buffer);

                buffer_.push(target_buffer.data(), (target_buffer.size() + 1) / 2);
            }
        }

        std::size_t frame_to_wrote = buffer_.pop(buffer, frame_count * channels_) / channels_;

        samples_copied_ += frame_to_wrote * channels_;

        std::size_t sample_to_wrote = frame_to_wrote * channels_;
        std::size_t size_to_wrote = frame_to_wrote * channels_ * sizeof(std::int16_t);

        // If the amount of buffer left is deemed to be insufficient (this takes account of current frame count that is needed)
        if (internal_decode_running_out()) {
            if (!more_requested) {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                if (more_buffer_callback_) {
                    more_buffer_callback_(more_buffer_userdata_);
                }
                
                more_requested = true;
            }
        }

        samples_played_ += sample_to_wrote;
        frame_wrote += frame_to_wrote;

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

    dsp_input_stream_shared::dsp_input_stream_shared(drivers::audio_driver *aud)
        : dsp_input_stream()
        , aud_(aud)
        , stream_(nullptr)
        , read_bytes_(0) {

    }

    dsp_input_stream_shared::~dsp_input_stream_shared() {
        if (stream_) {
            stream_->stop();
        }
    }

    bool dsp_input_stream_shared::set_properties(const std::uint32_t freq, const std::uint8_t channels) {
        if ((channels_ == channels) && (freq_ == freq)) {
            return true;
        }

        if ((channels == 0) || (freq == 0)) {
            return true;
        }

        bool was_recording = stream_ && stream_->is_recording();

        if (stream_) {
            stream_->stop();
            stream_.reset();
        }

        channels_ = channels;
        freq_ = freq;

        stream_ = aud_->new_input_stream(freq, channels, [this](std::int16_t *buffer, const std::size_t nb_frames) {
            return this->record_data_callback(buffer, nb_frames);
        });

        if (was_recording) {
            stream_->start();
        }

        return true;
    }

    bool dsp_input_stream_shared::start() {
        if (!stream_) {
            channels_ = 1;
            freq_ = 8000;

            // Create default stream. This follows default MMFDevSound default setting closely
            // Even though this is a generic stream... ;)
            stream_ = aud_->new_input_stream(8000, 1, [this](std::int16_t *buffer, const std::size_t nb_frames) {
                return this->record_data_callback(buffer, nb_frames);
            });
        }

        if (stream_->is_recording()) {
            return false;
        }

        return stream_->start();
    }

    bool dsp_input_stream_shared::stop() {
        if (stream_ && stream_->is_recording()) {
            bool result = stream_->stop();

            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                if (complete_callback_) {
                    complete_callback_(complete_userdata_);
                }
            }

            return result;
        }

        return false;
    }

    std::uint64_t dsp_input_stream_shared::real_time_position() {
        std::uint64_t frame_streamed = 0;
        if (!stream_->current_frame_position(&frame_streamed)) {
            LOG_ERROR(DRIVER_AUD, "Fail to retrieve streamed sample count!");
            return 0;
        }

        return frame_streamed * channels_ * 1000000ULL / freq_;
    }

    std::uint64_t dsp_input_stream_shared::position() {
        return samples_played_ * 1000000ULL / freq_;
    }

    std::size_t dsp_input_stream_shared::record_data_callback(std::int16_t *buffer, std::size_t frames) {
        if (read_queue_.empty()) {
            ring_buffer_.push(buffer, frames * channels_);
            return frames;
        }

        const input_read_request &request = read_queue_.front();

        if (ring_buffer_.size() != 0) {
            std::uint32_t max_copy = ((read_bytes_ + ring_buffer_.size() * sizeof(std::uint16_t)) >= request.second) ? static_cast<std::uint32_t>(request.second - read_bytes_)
                : static_cast<std::uint32_t>(ring_buffer_.size() * sizeof(std::uint16_t));

            ring_buffer_.pop(request.first + read_bytes_, max_copy / sizeof(std::uint16_t));
            read_bytes_ += max_copy;
        }

        std::size_t bytes_here = frames * channels_ * sizeof(std::int16_t);
        std::uint32_t bytes_to_copy = 0;
        std::size_t bytes_left = bytes_here;

        if (read_bytes_ < request.second) {
            bytes_to_copy = ((read_bytes_ + bytes_here) >= request.second) ? static_cast<std::uint32_t>(request.second - read_bytes_)
                : static_cast<std::uint32_t>(bytes_here);

            if (bytes_to_copy != 0) {
                std::memcpy(request.first + read_bytes_, buffer, bytes_to_copy);
            }

            bytes_left = bytes_here - bytes_to_copy;
        }

        if (bytes_left > 0) {
            ring_buffer_.push(buffer + (bytes_to_copy / sizeof(std::int16_t)), bytes_left / sizeof(std::int16_t)); 
        }

        if (bytes_to_copy + read_bytes_ >= request.second) {
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                if (more_buffer_callback_) {
                    more_buffer_callback_(more_buffer_userdata_);
                }
            }

            read_bytes_ = 0;
            read_queue_.pop();
        } else {
            read_bytes_ += bytes_to_copy;
        }

        return frames;
    }

    bool dsp_input_stream_shared::read(std::uint8_t *data, const std::uint32_t max_data_size) {
        read_queue_.push(std::make_pair(data, max_data_size));
        return true;
    }
}