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
        : aud_(aud)
        , pointer_(0) {
        last_frame_[0] = 0;
        last_frame_[1] = 0;
    }

    dsp_output_stream_shared::~dsp_output_stream_shared() {
        if (stream_) {
            stream_->stop();
        }
    }

    bool dsp_output_stream_shared::set_properties(const std::uint32_t freq, const std::uint8_t channels) {
        if (stream_ && stream_->is_playing()) {
            return false;
        }

        channels_ = channels;

        stream_ = aud_->new_output_stream(freq, channels, [this](std::int16_t *buffer, const std::size_t nb_frames) {
            return data_callback(buffer, nb_frames);
        });

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
        if (!stream_)
            return true;

        return stream_->start();
    }

    bool dsp_output_stream_shared::stop() {
        if (!stream_)
            return true;

        // Call the finish callback
        if (complete_callback_)
            complete_callback_(complete_userdata_);

        const bool stop_result = stream_->stop();
        
        // Discard all buffers
        while (auto buffer = buffers_.pop()) {
        }

        return stop_result;
    }

    void dsp_output_stream_shared::register_callback(dsp_stream_notification_type nof_type, dsp_stream_notification_callback callback,
        void *userdata) {
        const std::lock_guard<std::mutex> guard(callback_lock_);

        switch (nof_type) {
        case dsp_stream_notification_done:
            complete_callback_ = callback;
            complete_userdata_ = userdata;
            break;

        case dsp_stream_notification_buffer_copied:
            buffer_copied_userdata_ = userdata;
            buffer_copied_callback_ = callback;
            break;

        default:
            LOG_ERROR("Unsupport notification type!");
            break;
        }
    }

    void *dsp_output_stream_shared::get_userdata(dsp_stream_notification_type nof_type) {
        switch (nof_type) {
        case dsp_stream_notification_done:
            return complete_userdata_;

        case dsp_stream_notification_buffer_copied:
            return buffer_copied_userdata_;

        default:
            LOG_ERROR("Unsupport notification type!");
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

    std::size_t dsp_output_stream_shared::data_callback(std::int16_t *buffer, const std::size_t frame_count) {
        std::size_t frame_wrote = 0;

        while (frame_wrote < frame_count) {
            std::optional<dsp_buffer> encoded;

            if ((decoded_.size() == 0) || (decoded_.size() == pointer_)) {
                encoded = buffers_.pop();

                if (!encoded) {
                    break;
                }
                        
                // Callback that internal buffer has been copied
                {
                    const std::lock_guard<std::mutex> guard(callback_lock_);
                    if (buffer_copied_callback_) {
                        buffer_copied_callback_(buffer_copied_userdata_);
                        buffer_copied_callback_ = nullptr;
                    }
                }

                pointer_ = 0;

                if (format_ == PCM16_FOUR_CC_CODE) {
                    decoded_ = encoded.value();
                } else {
                    decode_data(encoded.value(), decoded_);
                }
            }

            // We want to decode more
            std::size_t frame_to_wrote = ((decoded_.size() - pointer_) / channels_ / sizeof(std::int16_t));
            frame_to_wrote = std::min<std::size_t>(frame_to_wrote, frame_count - frame_wrote);

            std::memcpy(&buffer[frame_wrote * channels_], &decoded_[pointer_], frame_to_wrote * channels_ * sizeof(std::int16_t));

            // Set last frame
            std::memcpy(last_frame_, &decoded_[pointer_ + (frame_to_wrote - 1) * channels_ * sizeof(std::int16_t)], channels_ * sizeof(std::int16_t));

            pointer_ += frame_to_wrote * channels_ * sizeof(std::int16_t);
            frame_wrote += frame_to_wrote;
        }

        for (; frame_wrote < frame_count; frame_wrote++) {
            // We dont want to drain the audio driver, so fill it with last frame
            std::memcpy(&buffer[frame_wrote * channels_], last_frame_, channels_ * sizeof(std::int16_t));
        }

        // TODO: What? Is this right
        samples_played_ = frame_count * channels_;

        return frame_count;
    }
}