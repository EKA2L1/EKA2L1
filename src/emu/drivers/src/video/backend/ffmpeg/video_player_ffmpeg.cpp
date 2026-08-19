/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <drivers/video/backend/ffmpeg/video_player_ffmpeg.h>
#include <drivers/audio/audio.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/time.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <deque>

namespace eka2l1::drivers {
    // Read-ahead bounds for the decode loop. Video packets are buffered so that
    // reading is never throttled by the video display pacing (which used to leave
    // less than one video frame worth of audio queued, causing audio underruns).
    static constexpr std::size_t MAX_PENDING_IMAGE_PACKETS = 64;
    static constexpr std::size_t MAX_PENDING_AUDIO_PACKETS = 96;

    // Free slots that must be available in the decoded sample ring before another
    // audio packet is decoded into it. Covers the largest common frame (AAC:
    // 2048 samples/channel, stereo) with margin.
    static constexpr std::size_t AUDIO_RING_DECODE_HEADROOM = 8192;

    video_player_ffmpeg::video_player_ffmpeg(audio_driver *driver)
        : video_player()
        , stream_(nullptr)
        , should_stop_(false)
        , aud_driver_(driver)
        , format_ctx_(nullptr)
        , audio_codec_ctx_(nullptr)
        , image_codec_ctx_(nullptr)
        , temp_audio_frame_(nullptr)
        , resample_context_(nullptr)
        , audio_stream_index_(-1)
        , image_stream_index_(-1)
        , volume_(10)
        , fps_(1.0f) {
    }

    video_player_ffmpeg::~video_player_ffmpeg() {
        stop();
        reset_contexts();

        if (temp_audio_frame_) {
            av_frame_free(&temp_audio_frame_);
        }
    }

    void video_player_ffmpeg::reset_contexts() {
        stream_.reset();
        pending_samples_.reset();

        if (format_ctx_) {
            avformat_close_input(&format_ctx_);
        }

        if (audio_codec_ctx_) {
            avcodec_free_context(&audio_codec_ctx_);
        }

        if (image_codec_ctx_) {
            avcodec_free_context(&image_codec_ctx_);
        }

        if (resample_context_) {
            swr_free(&resample_context_);
        }

        audio_stream_index_ = -1;
        image_stream_index_ = -1;
        fps_ = 1.0f;
    }

    bool video_player_ffmpeg::prepare_codecs() {
        int result = avformat_find_stream_info(format_ctx_, nullptr);
        if (result < 0) {
            LOG_ERROR(DRIVER_VID, "Find video stream info failed with code {}", result);
            return false;
        }

        result = av_find_best_stream(format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (result < 0) {
            LOG_INFO(DRIVER_VID, "Video does not contain image stream, no image output will be displayed!");
        } else {
            image_stream_index_ = result;
        }

        result = av_find_best_stream(format_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (result < 0) {
            LOG_INFO(DRIVER_AUD, "Video does not contain audio stream, no audio will be ouputted");
        } else {
            audio_stream_index_ = result;
        }

        if ((image_stream_index_ < 0) && (audio_stream_index_ < 0)) {
            LOG_ERROR(DRIVER_VID, "Video does not have both image and audio. What do you gonna do? Show me? Grzz Bark bark");
            return false;
        }

        if (image_stream_index_ >= 0) {
            AVStream *image_stream = format_ctx_->streams[image_stream_index_];
            const AVCodec *image_codec = avcodec_find_decoder(image_stream->codecpar->codec_id);

            if (!image_codec) {
                LOG_ERROR(DRIVER_VID, "Can't find suitable decoder to decode video's image!");
            } else {
                image_codec_ctx_ = avcodec_alloc_context3(image_codec);
                if (!image_codec_ctx_) {
                    LOG_ERROR(DRIVER_VID, "Unable to allocate image decode context!");
                } else {
                    avcodec_parameters_to_context(image_codec_ctx_, image_stream->codecpar);
                    result = avcodec_open2(image_codec_ctx_, image_codec, nullptr);

                    if (result < 0) {
                        LOG_ERROR(DRIVER_VID, "Unable to open image decode context!");
                        avcodec_free_context(&image_codec_ctx_);
                    }

                    fps_ = static_cast<float>(av_q2d(image_stream->r_frame_rate));
                }
            }
        }

        if (audio_stream_index_ >= 0) {
            AVStream *audio_stream = format_ctx_->streams[audio_stream_index_];
            const AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);

            if (!audio_codec) {
                LOG_ERROR(DRIVER_VID, "Can't find suitable decoder to decode video's image!");
            } else {
                audio_codec_ctx_ = avcodec_alloc_context3(audio_codec);
                if (!audio_codec_ctx_) {
                    LOG_ERROR(DRIVER_VID, "Unable to allocate image decode context!");
                } else {
                    avcodec_parameters_to_context(audio_codec_ctx_, audio_stream->codecpar);
                    result = avcodec_open2(audio_codec_ctx_, audio_codec, nullptr);

                    if (result < 0) {
                        LOG_ERROR(DRIVER_VID, "Unable to open audio decode context!");
                        avcodec_free_context(&audio_codec_ctx_);
                    }

                    // Create new audio stream. Don't play yet
                    stream_ = aud_driver_->new_output_stream(audio_stream->codecpar->sample_rate,
                        audio_stream->codecpar->channels, [this](std::int16_t *output, std::size_t frames) {
                            return this->video_audio_callback(output, frames);
                        });

                    if (!stream_) {
                        LOG_ERROR(DRIVER_VID, "Error while create audio output stream!");
                        avcodec_free_context(&audio_codec_ctx_);
                    } else {
                        stream_->set_volume(volume_ / 10.0f);
                    }
                }
            }
        }

        if (!image_codec_ctx_ && !audio_codec_ctx_) {
            LOG_ERROR(DRIVER_VID, "Can't establish decoders for both audio and video!");
            return false;
        }

        return true;
    }

    bool video_player_ffmpeg::open_file(const std::string &path) {
        if (format_ctx_) {
            LOG_ERROR(DRIVER_VID, "Another video is still in play queue, can't open another one!");
            return false;
        }

        int result = avformat_open_input(&format_ctx_, path.c_str(), nullptr, nullptr);
        if (result < 0) {
            LOG_ERROR(DRIVER_VID, "Open new video input failed with code {}", result);
            return false;
        }

        if (!prepare_codecs()) {
            avformat_close_input(&format_ctx_);
            return false;
        }

        return true;
    }

    bool video_player_ffmpeg::open_custom_io(common::ro_stream &stream) {
        LOG_ERROR(DRIVER_VID, "Open video with custom IO is not yet supported!");
        return false;
    }

    void video_player_ffmpeg::close() {
        stop();
        reset_contexts();
    }

    void video_player_ffmpeg::play(const std::uint64_t *us_range) {
        if (us_range) {
            LOG_ERROR(DRIVER_VID, "Play with range is not yet supported. Playing the full video!");
        }

        stop();

        should_stop_ = false;
        done_event_.reset();

        // Leftover from a previous play. The stream is stopped here, so nobody
        // is consuming the ring.
        pending_samples_.reset();

        if (stream_) {
            stream_->start();
        }

        decode_thread_ = std::make_unique<std::thread>(&video_player_ffmpeg::video_audio_decode_loop, this);
    }

    void video_player_ffmpeg::pause() {

    }

    void video_player_ffmpeg::stop() {
        should_stop_ = true;

        if (stream_) {
            stream_->stop();
        }
        
        if (decode_thread_) {
            done_event_.set();
            decode_thread_->join();

            decode_thread_.reset();
        }

        while (std::optional<AVPacket*> packet = audio_packets_.pop()) {
            AVPacket *packet_unpacked = std::move(packet.value());
            av_packet_free(&packet_unpacked);
        }
    }

    std::uint32_t video_player_ffmpeg::max_volume() const {
        return 10;
    }
    
    std::uint32_t video_player_ffmpeg::volume() const {
        return volume_;
    }

    bool video_player_ffmpeg::set_volume(const std::uint32_t volume) {
        volume_ = common::clamp(0U, 10U, volume);
        if (stream_) {
            stream_->set_volume(volume_ / 10.0f);
        }

        return true;
    }

    std::uint64_t video_player_ffmpeg::duration() const {
        return 0;
    }

    std::uint64_t video_player_ffmpeg::position() const {
        return 0;
    }

    void video_player_ffmpeg::set_position(const std::uint64_t pos) {

    }

    void video_player_ffmpeg::set_fps(const float fps) {
        fps_ = fps;
    }

    float video_player_ffmpeg::get_fps() const {
        return fps_;
    }

    std::uint32_t video_player_ffmpeg::audio_bitrate() const {
        return 0;
    }

    std::uint32_t video_player_ffmpeg::video_bitrate() const {
        return 0;
    }

    eka2l1::vec2 video_player_ffmpeg::get_video_size() const {
        if (!image_codec_ctx_) {
            return eka2l1::vec2(0, 0);
        }

        return eka2l1::vec2(image_codec_ctx_->width, image_codec_ctx_->height);
    }

    std::size_t video_player_ffmpeg::video_audio_callback(std::int16_t *output_buffer, std::size_t frames) {
        const std::uint8_t channel_count = stream_->get_channels();
        const std::size_t sample_total_count = channel_count * frames;

        // Fill all zero first in case we got nothing. Decoding happens on the decode
        // thread: this callback runs on the realtime audio thread and only drains
        // the sample ring.
        std::fill(output_buffer, output_buffer + sample_total_count, 0);

        pending_samples_.pop(output_buffer, common::min(sample_total_count, pending_samples_.size()));
        return frames;
    }

    void video_player_ffmpeg::decode_pending_audio() {
        if (!audio_codec_ctx_ || !stream_) {
            return;
        }

        const std::uint8_t channel_count = stream_->get_channels();

        while ((pending_samples_.capacity() - pending_samples_.size()) >= AUDIO_RING_DECODE_HEADROOM) {
            std::optional<AVPacket*> packet_op = audio_packets_.pop();
            if (!packet_op.has_value()) {
                break;
            }

            AVPacket *packet = std::move(packet_op.value());
            int result = avcodec_send_packet(audio_codec_ctx_, packet);
            av_packet_free(&packet);

            if (result < 0) {
                LOG_ERROR(DRIVER_VID, "Sending video's audio packet failed with code {}", result);
                continue;
            }

            if (!temp_audio_frame_) {
                temp_audio_frame_ = av_frame_alloc();
            }

            while (avcodec_receive_frame(audio_codec_ctx_, temp_audio_frame_) >= 0) {
                if (!resample_context_) {
                    const int dest_channel_type = (channel_count == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
                    AVStream *audio_stream = format_ctx_->streams[audio_stream_index_];

                    resample_context_ = swr_alloc_set_opts(nullptr, dest_channel_type, AV_SAMPLE_FMT_S16, stream_->get_sample_rate(),
                        audio_stream->codecpar->channel_layout, static_cast<AVSampleFormat>(audio_stream->codecpar->format), audio_stream->codecpar->sample_rate,
                        0, nullptr);

                    if (swr_init(resample_context_) < 0) {
                        LOG_ERROR(DRIVER_AUD, "Error initializing audio resample context!");
                        swr_free(&resample_context_);

                        return;
                    }
                }

                std::vector<std::uint16_t> data_temp(channel_count * temp_audio_frame_->nb_samples);

                std::uint8_t *data_temp_ptr = reinterpret_cast<std::uint8_t*>(data_temp.data());
                const std::uint8_t **source = const_cast<const std::uint8_t**>(temp_audio_frame_->extended_data);

                result = swr_convert(resample_context_, &data_temp_ptr, temp_audio_frame_->nb_samples, source,
                    temp_audio_frame_->nb_samples);

                if (result < 0) {
                    LOG_ERROR(DRIVER_VID, "Unable to resample video audio to PCM16 format!");
                    break;
                }

                pending_samples_.push(data_temp.data(), static_cast<std::size_t>(result) * channel_count);
            }
        }
    }

    void video_player_ffmpeg::video_audio_decode_loop() {
        AVPacket *temp_packet = av_packet_alloc();
        AVFrame *temp_frame = av_frame_alloc();
        AVFrame *scaled_frame = av_frame_alloc();

        last_update_us_ = 0;

        struct SwsContext *scale_context = nullptr;
        std::uint8_t *frame_buffer = nullptr;
        std::size_t frame_buffer_size = 0;

        std::uint64_t amount_to_sleep = static_cast<std::uint64_t>(common::microsecs_per_sec / fps_);
        bool complete_callback_called = false;

        // Video packets read ahead of their display time. Reading must not be
        // throttled by the video pacing sleep below, else the audio stream is fed
        // with less than one video frame worth of samples of margin and underruns
        // (audible stutter) whenever an iteration overshoots the frame period.
        std::deque<AVPacket*> pending_image_packets;

        const bool has_image_decoder = image_codec_ctx_ && image_frame_available_callback_;
        const bool has_audio_output = audio_codec_ctx_ && stream_;

        // Target decoded-audio backlog before the reader relaxes (~0.5s).
        const std::size_t audio_samples_low_water = has_audio_output
            ? common::min<std::size_t>(static_cast<std::size_t>(stream_->get_sample_rate()) * stream_->get_channels() / 2,
                pending_samples_.capacity() / 2)
            : 0;

        bool eof_reached = false;
        bool read_failed = false;

        while (!should_stop_) {
            // Read ahead until a video frame is in hand and the audio backlog is
            // comfortable (or the safety caps are hit).
            while (!eof_reached && !should_stop_ && (pending_image_packets.size() < MAX_PENDING_IMAGE_PACKETS)) {
                // Hard cap: never hold more compressed audio than this, even while
                // still hunting for a video packet (the video stream may simply
                // end before the audio stream does).
                if (audio_packets_.size() >= MAX_PENDING_AUDIO_PACKETS) {
                    break;
                }

                const bool audio_buffered_enough = !has_audio_output
                    || (pending_samples_.size() >= audio_samples_low_water);

                const bool image_satisfied = !has_image_decoder || !pending_image_packets.empty();

                if (audio_buffered_enough && image_satisfied) {
                    break;
                }

                int result = av_read_frame(format_ctx_, temp_packet);
                if (result < 0) {
                    if (result == AVERROR(EAGAIN)) {
                        continue;
                    }

                    eof_reached = true;
                    read_failed = (result != AVERROR_EOF);

                    break;
                }

                if ((temp_packet->stream_index == image_stream_index_) && has_image_decoder) {
                    pending_image_packets.push_back(av_packet_clone(temp_packet));
                } else if ((temp_packet->stream_index == audio_stream_index_) && has_audio_output) {
                    audio_packets_.push(av_packet_clone(temp_packet));
                }

                av_packet_unref(temp_packet);
            }

            if (read_failed) {
                should_stop_ = true;
                LOG_ERROR(DRIVER_VID, "Unable to read the current video frame!");

                if (play_complete_callback_) {
                    play_complete_callback_(play_complete_callback_userdata_, -1);
                    complete_callback_called = true;
                }

                break;
            }

            // Top up the decoded sample ring that the realtime audio callback drains.
            decode_pending_audio();

            if (!pending_image_packets.empty()) {
                AVPacket *image_packet = pending_image_packets.front();
                pending_image_packets.pop_front();

                int result = avcodec_send_packet(image_codec_ctx_, image_packet);
                av_packet_free(&image_packet);

                if (result < 0) {
                    LOG_ERROR(DRIVER_VID, "Error while sending video frame packet to decoder!");
                    should_stop_ = true;

                    if (play_complete_callback_) {
                        play_complete_callback_(play_complete_callback_userdata_, -1);
                        complete_callback_called = true;
                    }

                    break;
                }
                
                while (result >= 0) {
                    result = avcodec_receive_frame(image_codec_ctx_, temp_frame);
                    if ((result == AVERROR(EAGAIN)) || (result == AVERROR_EOF)) {
                        break;
                    }

                    if (!scale_context) {
                        // Setup frame that will store scaled result
                        scale_context = sws_getContext(image_codec_ctx_->width, image_codec_ctx_->height,
                            image_codec_ctx_->pix_fmt, image_codec_ctx_->width, image_codec_ctx_->height, AV_PIX_FMT_RGBA,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);

                        int total_bytes_buffer = av_image_get_buffer_size(AV_PIX_FMT_RGBA, image_codec_ctx_->width,
                           image_codec_ctx_->height, 32);

                        // I put it extra, suspect sws_scale is heap corrupting the data a bit
                        frame_buffer = reinterpret_cast<std::uint8_t*>(av_malloc(total_bytes_buffer + (total_bytes_buffer / 2)));

                        av_image_fill_arrays(scaled_frame->data, scaled_frame->linesize, frame_buffer, AV_PIX_FMT_RGBA, image_codec_ctx_->width,
                            image_codec_ctx_->height, 32);

                        frame_buffer_size = static_cast<std::size_t>(total_bytes_buffer);
                    }
                    
                    int resres = sws_scale(scale_context, temp_frame->data, temp_frame->linesize, 0, temp_frame->height, scaled_frame->data,
                        scaled_frame->linesize);

                    image_frame_available_callback_(image_frame_available_callback_userdata_, frame_buffer, frame_buffer_size);
                    
                    // Syncing FPS
                    const std::uint64_t current_time_us = common::get_current_utc_time_in_microseconds_since_epoch();
                    const std::uint64_t delta = current_time_us - last_update_us_;

                    if (delta < amount_to_sleep) {
                        std::this_thread::sleep_for(std::chrono::microseconds(amount_to_sleep - delta));
                    }

                    last_update_us_ = common::get_current_utc_time_in_microseconds_since_epoch();
                }
            } else if (eof_reached) {
                // Everything read. Let the buffered audio finish playing before
                // reporting completion, since reading ran ahead of realtime.
                const bool audio_drained = !has_audio_output
                    || ((audio_packets_.size() == 0) && (pending_samples_.size() == 0))
                    || !stream_->is_playing();

                if (audio_drained) {
                    should_stop_ = true;

                    if (play_complete_callback_) {
                        play_complete_callback_(play_complete_callback_userdata_, 0);
                        complete_callback_called = true;
                    }

                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } else {
                // Audio-only content (or no displayable frame yet): pace gently while
                // keeping the sample ring topped up.
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        while (!pending_image_packets.empty()) {
            AVPacket *leftover = pending_image_packets.front();
            pending_image_packets.pop_front();

            av_packet_free(&leftover);
        }

        // Wait until a confirmation that I can exit
        done_event_.wait();

        if (scale_context) {
            sws_freeContext(scale_context);
        }

        if (scaled_frame) {
            av_frame_free(&scaled_frame);
        }

        if (temp_frame) {
            av_frame_free(&temp_frame);
        }

        if (temp_packet) {
            av_packet_free(&temp_packet);
        }

        av_free(frame_buffer);

        if (!complete_callback_called) {            
            if (play_complete_callback_) {
                play_complete_callback_(play_complete_callback_userdata_, 0);
            }
        }
    }
}