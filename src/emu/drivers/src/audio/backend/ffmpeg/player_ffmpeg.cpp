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
#include <drivers/audio/backend/ffmpeg/player_ffmpeg.h>

extern "C" {
#include <libswresample/swresample.h>
}

namespace eka2l1::drivers {
    void player_ffmpeg_request::deinit() {
        if (type_ == player_request_format) {
            av_free_packet(&packet_);
            avformat_free_context(format_);

            codec_ = nullptr;
        }
    }

    player_ffmpeg_request::~player_ffmpeg_request() {
        deinit();
    }

    static bool open_ffmpeg_stream(player_ffmpeg_request &request) {
        if (avformat_find_stream_info(request.format_, nullptr) < 0) {
            LOG_ERROR("Error while finding stream info of input {}", request.url_);
            avformat_free_context(request.format_);

            return false;
        }

        av_init_packet(&request.packet_);

        // Trying to get stream audio codec
        if (!request.format_->audio_codec) {
            // Getting it ourself smh
            request.format_->audio_codec = avcodec_find_decoder(request.format_->audio_codec_id);

            if (!request.format_->audio_codec) {
                AVOutputFormat *format = av_guess_format(nullptr, request.url_.c_str(), nullptr);

                if (!format) {
                    LOG_ERROR("Error while finding responsible audio decoder of input {}", request.url_);
                    avformat_free_context(request.format_);
                    av_free_packet(&request.packet_);

                    return false;
                }

                request.format_->audio_codec_id = format->audio_codec;
                request.format_->audio_codec = avcodec_find_decoder(request.format_->audio_codec_id);
            }
        }

        // Trying to open decoder now
        AVCodecContext *preallocated_context = nullptr;

        // Iterate all stream to find first audio stream to get context
        for (std::uint32_t i = 0; i < request.format_->nb_streams; i++) {
            preallocated_context = request.format_->streams[i]->codec;

            if (preallocated_context->codec_type == AVMEDIA_TYPE_AUDIO) {
                break;
            }
        }

        // Open the context
        request.codec_ = preallocated_context;

        if (avcodec_open2(request.codec_, request.format_->audio_codec, nullptr) < 0) {
            LOG_ERROR("Unable to open codec of stream url {}", request.url_);

            avformat_free_context(request.format_);
            av_free_packet(&request.packet_);

            return false;
        }

        request.channels_ = request.codec_->channels;
        request.freq_ = request.codec_->sample_rate;

        return true;
    }

    bool player_ffmpeg::queue_url(const std::string &url) {
        player_request_instance request = std::make_unique<player_ffmpeg_request>();
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());

        request_ff->url_ = url;
        request_ff->format_ = avformat_alloc_context();
        request_ff->type_ = player_request_format;

        if (avformat_open_input(&request_ff->format_, url.c_str(), nullptr, nullptr) < 0) {
            LOG_ERROR("Error while opening AVFormat Input!");
            avformat_free_context(request_ff->format_);

            return false;
        }

        if (!open_ffmpeg_stream(*request_ff)) {
            return false;
        }

        requests_.push(std::move(request));

        return true;
    }

    void player_ffmpeg::reset_request(player_request_instance &request) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());

        avformat_flush(request_ff->format_);

        if (avformat_seek_file(request_ff->format_, -1, INT64_MIN, 0, INT64_MAX, 0) < 0) {
            LOG_ERROR("Error seeking the stream!");
        }
    }

    void player_ffmpeg::get_more_data(player_request_instance &request) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());

        // Data drain, try to get more
        if (request_ff->type_ == player_request_format) {
            if (request_ff->flags_ & 1) {
                output_stream_->stop();
                return;
            }

            if (av_read_frame(request_ff->format_, &request_ff->packet_) < 0) {
                request_ff->flags_ |= 1;
                return;
            }

            // Send packet to decoder
            if (avcodec_send_packet(request_ff->codec_, &request_ff->packet_) >= 0) {
                AVFrame *frame = av_frame_alloc();

                if (avcodec_receive_frame(request_ff->codec_, frame) < 0) {
                    LOG_ERROR("Error while decoding a frame!");
                    av_frame_free(&frame);
                    request_ff->flags_ |= 1;
                    return;
                }

                std::size_t base_ptr = 0;

                if (request_ff->use_push_new_data_) {
                    base_ptr = request_ff->data_.size();
                    request_ff->use_push_new_data_ = false;
                }

                request_ff->data_.resize(base_ptr + request_ff->channels_ * frame->nb_samples * sizeof(std::int16_t));

                if ((frame->format != AV_SAMPLE_FMT_S16) || (frame->channels != request_ff->channels_)
                    || (frame->sample_rate != request_ff->freq_)) {
                    // Resample it
                    const int dest_channel_type = (request_ff->channels_ == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

                    SwrContext *swr = swr_alloc_set_opts(nullptr,
                        dest_channel_type, AV_SAMPLE_FMT_S16, request_ff->freq_,
                        frame->channel_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                        0, nullptr);

                    if (swr_init(swr) < 0) {
                        LOG_ERROR("Error initializing SWR context");
                        av_frame_free(&frame);

                        return;
                    }

                    std::uint8_t *output = request_ff->data_.data() + base_ptr;
                    const std::uint8_t *source = frame->data[0];

                    const int result = swr_convert(swr, &output, frame->nb_samples, &source, frame->nb_samples);
                    swr_free(&swr);

                    if (result < 0) {
                        LOG_ERROR("Error resample audio data!");
                        request_ff->flags_ |= 1;
                        return;
                    }
                } else {
                    // Just gonna copy smh
                    std::memcpy(&request_ff->data_[base_ptr], frame->data[0], request_ff->data_.size());
                }

                av_frame_free(&frame);
            }
        }

        request_ff->data_pointer_ = 0;
    }

    bool player_ffmpeg::queue_data(const char *raw_data, const std::size_t data_size,
        const std::uint32_t encoding_type, const std::uint32_t frequency,
        const std::uint32_t channels) {
        return true;
    }

    player_ffmpeg::player_ffmpeg(audio_driver *driver)
        : player_shared(driver) {
    }

    player_ffmpeg::~player_ffmpeg() {
    }
}