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

#include <common/algorithm.h>
#include <common/log.h>
#include <drivers/audio/backend/dsp_shared.h>

namespace eka2l1::drivers {
    dsp_output_stream_shared::dsp_output_stream_shared(drivers::audio_driver *aud)
        : dsp_output_stream()
        , aud_(aud)
        , virtual_stop(true)
        , more_requested(false)
        , last_write_samples_(0)
        , avg_frame_count_(0) {
    }

    void dsp_output_stream_shared::shutdown_stream() {
        if (stream_) {
            stream_->stop();
            stream_.reset();
        }
    }

    dsp_output_stream_shared::~dsp_output_stream_shared() {
        // Safety net for a stream that a derived class did not tear down itself.
        // The most-derived destructor is expected to have already called
        // shutdown_stream() while its vtable was still intact (see the header);
        // by this point calling back into a virtual would be unsafe, so this
        // only stops a stream that is somehow still alive.
        shutdown_stream();
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
        last_write_samples_.store(0, std::memory_order_relaxed);

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

        last_write_samples_.store((data_size + 1) / 2, std::memory_order_relaxed);

        more_requested = false;
        return true;
    }

    std::size_t dsp_output_stream_shared::low_water_mark_samples() const {
        // Real MMF hands the client buffer to the sound device as soon as it arrives and
        // completes the copy notification right away, so the client always has a whole
        // buffer period to prepare the next one. Only asking for more once the ring is
        // nearly dry (four render callbacks, tens of milliseconds) shrinks that budget to
        // almost nothing: a client that misses the window once sees the stream run dry,
        // reports KErrUnderflow and - Puyo Pop on the N-Gage does exactly this - never
        // streams again. Keep a whole guest buffer queued ahead instead, so the guest gets
        // its own buffer period to answer, and cap the lookahead so a title writing huge
        // buffers does not turn into seconds of latency.
        const std::size_t hardware_low = avg_frame_count_ * channels_ * 4;
        const std::size_t half_second = static_cast<std::size_t>(freq_) * common::max<std::size_t>(channels_, 1) / 2;
        const std::size_t guest_buffer = common::min<std::size_t>(
            last_write_samples_.load(std::memory_order_relaxed), half_second);

        return common::max(hardware_low, guest_buffer);
    }

    bool dsp_output_stream_shared::internal_decode_running_out() {
        if (format_ == PCM16_FOUR_CC_CODE) {
            return (buffer_.size() <= low_water_mark_samples());
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

        samples_copied_.fetch_add(frame_to_wrote * channels_, std::memory_order_relaxed);

        std::size_t sample_to_wrote = frame_to_wrote * channels_;
        std::size_t size_to_wrote = frame_to_wrote * channels_ * sizeof(std::int16_t);

        // If the amount of buffer left is deemed to be insufficient (this takes account of current frame count that is needed)
        if (internal_decode_running_out()) {
            // Claim the request slot before invoking the callback, not after.
            // The callback wakes the guest thread, which may hand us data --
            // and clear this flag via write() -- before the call even returns.
            // Publishing the flag afterwards would overwrite that clear, so no
            // further data would ever be requested and the stream would starve.
            if (!more_requested.exchange(true)) {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                if (more_buffer_callback_ && !more_buffer_callback_(more_buffer_userdata_)) {
                    // The request did not go through, so nothing will clear the
                    // flag for us; release it so the next callback can retry.
                    more_requested = false;
                }
            }
        }

        samples_played_.fetch_add(sample_to_wrote, std::memory_order_relaxed);
        frame_wrote += frame_to_wrote;

        if (frame_wrote < frame_count) {
            std::memset(&buffer[frame_wrote * channels_], 0, (frame_count - frame_wrote) * channels_ * sizeof(std::int16_t));
        }

        return frame_count;
    }

    std::uint64_t dsp_output_stream_shared::position() {
        const std::uint32_t channels = common::max<std::uint32_t>(channels_, 1);
        return frames_to_microseconds(samples_played_.load(std::memory_order_relaxed) / channels, freq_);
    }

    std::uint64_t dsp_output_stream_shared::real_time_position() {
        // Counts the guest's own audio as it reaches the hardware, not wall clock. The
        // backing hardware stream has a free-running frame counter, but it keeps ticking
        // through the silence we pad an empty ring with and through a virtual stop, and it
        // survives reset_stat(). Reporting that as CMdaAudioOutputStream::Position() breaks
        // guests two ways: a title restarting a stream (reset_stat + stop + write, which is
        // what the media client patch does) reads the whole previous playback back, and a
        // title that paces its writes off the position - Puyo Pop feeds one buffer per
        // reported step - sees the position jump several buffers ahead whenever we are late
        // and stops feeding. samples_played_ only advances on samples actually taken out of
        // the ring and is cleared by reset_stat(), which is the position a real device
        // reports.
        return position();
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

            dsp_stream_notification_callback callback;
            dsp_stream_userdata userdata = nullptr;
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                callback = complete_callback_;
                userdata = complete_userdata_;
            }
            if (callback) {
                callback(userdata);
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
        const std::uint32_t channels = common::max<std::uint32_t>(channels_, 1);
        return frames_to_microseconds(samples_played_.load(std::memory_order_relaxed) / channels, freq_);
    }

    std::size_t dsp_input_stream_shared::record_data_callback(std::int16_t *buffer, std::size_t frames) {
        input_read_request completed_request{};
        bool request_completed = false;

        {
            const std::lock_guard<std::mutex> state_guard(input_state_lock_);
            samples_played_.fetch_add(frames * channels_, std::memory_order_relaxed);

            if (read_queue_.empty()) {
                ring_buffer_.push(buffer, frames * channels_);
                return frames;
            }

            const input_read_request &request = read_queue_.front();

            if (ring_buffer_.size() != 0) {
                const std::uint32_t max_copy = ((read_bytes_ + ring_buffer_.size() * sizeof(std::uint16_t)) >= request.second)
                    ? static_cast<std::uint32_t>(request.second - read_bytes_)
                    : static_cast<std::uint32_t>(ring_buffer_.size() * sizeof(std::uint16_t));

                ring_buffer_.pop(request.first + read_bytes_, max_copy / sizeof(std::uint16_t));
                read_bytes_ += max_copy;
            }

            const std::size_t bytes_here = frames * channels_ * sizeof(std::int16_t);
            std::uint32_t bytes_to_copy = 0;
            std::size_t bytes_left = bytes_here;

            if (read_bytes_ < request.second) {
                bytes_to_copy = ((read_bytes_ + bytes_here) >= request.second)
                    ? static_cast<std::uint32_t>(request.second - read_bytes_)
                    : static_cast<std::uint32_t>(bytes_here);

                if (bytes_to_copy != 0) {
                    std::memcpy(request.first + read_bytes_, buffer, bytes_to_copy);
                }

                bytes_left = bytes_here - bytes_to_copy;
            }

            if (bytes_left > 0) {
                ring_buffer_.push(buffer + (bytes_to_copy / sizeof(std::int16_t)),
                    bytes_left / sizeof(std::int16_t));
            }

            read_bytes_ += bytes_to_copy;
            if (read_bytes_ >= request.second) {
                completed_request = request;
                request_completed = true;
            }
        }

        if (request_completed) {
            bool notification_delivered = true;
            dsp_stream_notification_callback callback;
            dsp_stream_userdata userdata = nullptr;
            {
                const std::lock_guard<std::mutex> guard(callback_lock_);
                callback = more_buffer_callback_;
                userdata = more_buffer_userdata_;
            }
            if (callback) {
                notification_delivered = callback(userdata);
            }

            if (notification_delivered) {
                const std::lock_guard<std::mutex> state_guard(input_state_lock_);
                if (!read_queue_.empty() && (read_queue_.front() == completed_request)) {
                    read_bytes_ = 0;
                    read_queue_.pop();
                }
            }
        }

        return frames;
    }

    bool dsp_input_stream_shared::read(std::uint8_t *data, const std::uint32_t max_data_size) {
        const std::lock_guard<std::mutex> state_guard(input_state_lock_);
        read_queue_.push(std::make_pair(data, max_data_size));
        return true;
    }
}
