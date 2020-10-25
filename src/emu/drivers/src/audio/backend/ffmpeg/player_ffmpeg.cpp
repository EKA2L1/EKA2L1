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

#include <common/buffer.h>
#include <common/log.h>

#include <drivers/audio/backend/ffmpeg/player_ffmpeg.h>

extern "C" {
#include <libswresample/swresample.h>
}

namespace eka2l1::drivers {
    void player_ffmpeg_request::deinit() {
        av_free_packet(&packet_);
        avformat_free_context(format_context_);

        if (custom_io_) {
            avio_context_free(&custom_io_);
        }

        if (custom_io_buffer_) {
            av_free(custom_io_buffer_);
        }

        codec_ = nullptr;
    }

    player_ffmpeg_request::~player_ffmpeg_request() {
        deinit();
    }

    static bool open_ffmpeg_stream(player_ffmpeg_request &request) {
        if (avformat_find_stream_info(request.format_context_, nullptr) < 0) {
            LOG_ERROR("Error while finding stream info of input {}", request.url_);
            avformat_free_context(request.format_context_);

            return false;
        }

        av_init_packet(&request.packet_);

        // Trying to get stream audio codec
        if (!request.format_context_->audio_codec) {
            // Getting it ourself smh
            request.format_context_->audio_codec = avcodec_find_decoder(request.format_context_->audio_codec_id);

            if (!request.format_context_->audio_codec) {
                AVOutputFormat *format = av_guess_format(nullptr, request.url_.c_str(), nullptr);

                if (!format) {
                    LOG_ERROR("Error while finding responsible audio decoder of input {}", request.url_);
                    avformat_free_context(request.format_context_);
                    av_free_packet(&request.packet_);

                    return false;
                }

                request.format_context_->audio_codec_id = format->audio_codec;
                request.format_context_->audio_codec = avcodec_find_decoder(request.format_context_->audio_codec_id);
            }
        }

        // Trying to open decoder now
        AVCodecContext *preallocated_context = nullptr;

        // Iterate all stream to find first audio stream to get context
        for (std::uint32_t i = 0; i < request.format_context_->nb_streams; i++) {
            preallocated_context = request.format_context_->streams[i]->codec;

            if (preallocated_context->codec_type == AVMEDIA_TYPE_AUDIO) {
                break;
            }
        }

        // Open the context
        request.codec_ = preallocated_context;

        if (avcodec_open2(request.codec_, request.format_context_->audio_codec, nullptr) < 0) {
            LOG_ERROR("Unable to open codec of stream url {}", request.url_);

            avformat_free_context(request.format_context_);
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

        const std::lock_guard<std::mutex> guard(lock_);
        requests_.push(std::move(request));

        return true;
    }

    void player_ffmpeg::reset_request(player_request_instance &request) {
        if (!request) {
            return;
        }

        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());
        if (!request_ff->format_context_) {
            if (!make_backend_source(request)) {
                return;
            }
        }

        set_position_for_custom_format(request, 0);
    }
    
    bool player_ffmpeg::is_ready_to_play(player_request_instance &request) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());
        return request_ff && request_ff->format_context_;
    }

    void player_ffmpeg::get_more_data(player_request_instance &request) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());

        // Data drain, try to get more
        if (request_ff->flags_ & 1) {
            output_stream_->stop();
            return;
        }

        if (av_read_frame(request_ff->format_context_, &request_ff->packet_) < 0) {
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

        request_ff->data_pointer_ = 0;
    }

    static int ffmpeg_custom_rw_io_read(void *opaque, std::uint8_t *buf, int buf_size) {
        common::rw_stream *stream = reinterpret_cast<common::rw_stream*>(opaque);
        const std::uint64_t size = stream->read(buf, buf_size);

        return static_cast<int>(size);
    }

    static int ffmpeg_custom_rw_io_write(void *opaque, std::uint8_t *buf, int buf_size) {
        common::rw_stream *stream = reinterpret_cast<common::rw_stream*>(opaque);
        const std::uint64_t size = stream->write(buf, buf_size);

        return static_cast<int>(size);
    }
	
    std::int64_t ffmpeg_custom_rw_io_seek(void *opaque, int64_t offset, int whence) {
        whence &= ~AVSEEK_FORCE;
        common::seek_where pos_seek_mode = common::seek_where::beg;

        switch (whence) {
        case SEEK_SET:
            pos_seek_mode = common::seek_where::beg;
            break;

        case SEEK_CUR:
            pos_seek_mode = common::seek_where::cur;
            break;

        case SEEK_END:
            pos_seek_mode = common::seek_where::end;
            break;

        // Missing SEEK_SIZE support
        default:
            return -1;
        }

        common::rw_stream *stream = reinterpret_cast<common::rw_stream*>(opaque);
        stream->seek(offset, pos_seek_mode);

        return static_cast<std::int64_t>(stream->tell());
    }

    bool player_ffmpeg::queue_custom(common::rw_stream *the_stream) {
        player_request_instance request = std::make_unique<player_ffmpeg_request>();
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());

        static constexpr std::uint32_t CUSTOM_IO_BUFFER_SIZE = 8192;

        request_ff->custom_io_buffer_ = reinterpret_cast<std::uint8_t*>(av_malloc(CUSTOM_IO_BUFFER_SIZE));

        if (!request_ff->custom_io_buffer_) {
            return false;
        }

        request_ff->custom_io_ = avio_alloc_context(request_ff->custom_io_buffer_, CUSTOM_IO_BUFFER_SIZE,
            0, the_stream, ffmpeg_custom_rw_io_read, ffmpeg_custom_rw_io_write, ffmpeg_custom_rw_io_seek);

        if (!request_ff->custom_io_) {
            av_free(request_ff->custom_io_buffer_);
            return false;
        }

        const std::lock_guard<std::mutex> guard(lock_);
        
        requests_.push(std::move(request));
        return true;
    }

    bool player_ffmpeg::set_position_for_custom_format(player_request_instance &request, const std::uint64_t pos_in_us) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(request.get());

        avformat_flush(request_ff->format_context_);

        // The unit is microseconds
        if (avformat_seek_file(request_ff->format_context_, -1, INT64_MIN, pos_in_us, INT64_MAX, 0) < 0) {
            LOG_ERROR("Error seeking the stream!");
            return false;
        }

        return true;
    }

    bool player_ffmpeg::crop() {
        LOG_ERROR("Crop audio operation unsupported in FFMPEG backend");
        return false;
    }

    bool player_ffmpeg::record() {
        LOG_ERROR("Record audio operation unsupported in FFMPEG backend");
        return false;
    }

    bool player_ffmpeg::set_dest_freq(const std::uint32_t freq) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(requests_.front().get());

        if (!request_ff) {
            return false;
        }

        if (!request_ff->output_encoder_) {
            LOG_ERROR("No encoder is initialized!");
            return false;
        }

        const int *sample_rate_support_array = request_ff->output_encoder_->supported_samplerates;
        while (*sample_rate_support_array) {
            if (*sample_rate_support_array == static_cast<std::int32_t>(freq)) {
                request_ff->freq_ = static_cast<std::int32_t>(freq);
                return true;
            }
        }

        return false;
    }

    bool player_ffmpeg::set_dest_channel_count(const std::uint32_t cn) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(requests_.front().get());

        if (!request_ff) {
            return false;
        }

        if (!request_ff->output_encoder_) {
            LOG_ERROR("No encoder is initialized!");
            return false;
        }

        const std::uint64_t *layout_support_layout = request_ff->output_encoder_->channel_layouts;
        while (*layout_support_layout) {
            if (av_get_channel_layout_nb_channels(*layout_support_layout) == static_cast<std::int32_t>(cn)) {
                request_ff->channels_ = cn;
                request_ff->channel_layout_dest_ = *layout_support_layout;

                return true;
            }
        }

        return false;
    }

    static AVCodecID four_cc_to_av_codec_id(const std::uint32_t fcc) {
        switch (fcc) {
        case AUDIO_PCM16_CODEC_4CC:
            return AV_CODEC_ID_PCM_S16LE;

        case AUDIO_PCM8_CODEC_4CC:
            return AV_CODEC_ID_PCM_S8;

        case AUDIO_IMAD_CODEC_4CC:
            return AV_CODEC_ID_ADPCM_IMA_WAV;

        case AUDIO_MULAW_CODEC_4CC:
            return AV_CODEC_ID_PCM_MULAW;

        default:
            break;
        }

        return AV_CODEC_ID_NONE;
    }

    bool player_ffmpeg::set_dest_encoding(const std::uint32_t enc) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(requests_.front().get());

        if (!request_ff) {
            return false;
        }

        const AVCodecID codec_id = four_cc_to_av_codec_id(enc);

        if (codec_id == AV_CODEC_ID_NONE) {
            return false;
        }

        AVCodec *new_codec = avcodec_find_encoder(codec_id);
        if (!new_codec) {
            LOG_ERROR("Unable to find new encoder for codec id {}", codec_id);
            return false;
        }

        if (!(*new_codec->supported_samplerates) || !(*new_codec->channel_layouts)) {
            // One of those arrays is empty. Return
            LOG_ERROR("Supported sample rates or supported channel layouts array is empty!");
            return false;
        }

        request_ff->channels_ = av_get_channel_layout_nb_channels(*new_codec->channel_layouts);
        request_ff->freq_ = *new_codec->supported_samplerates;
        request_ff->encoding_ = enc;
        request_ff->output_encoder_ = new_codec;

        return true;
    }

    bool player_ffmpeg::make_backend_source(player_request_instance &request) {
        player_ffmpeg_request *request_ff = reinterpret_cast<player_ffmpeg_request *>(requests_.front().get());
        request_ff->format_context_ = avformat_alloc_context();

        if (request_ff->custom_io_) {
            request_ff->format_context_->pb = request_ff->custom_io_;
            request_ff->url_ = "Dummy";
        }

        auto do_free_custom = [](player_ffmpeg_request *request_ff) {
            if (request_ff->custom_io_) {
                avio_context_free(&request_ff->custom_io_);
                request_ff->custom_io_ = nullptr;
            }

            if (request_ff->custom_io_buffer_) {
                av_free(request_ff->custom_io_buffer_);
                request_ff->custom_io_buffer_ = nullptr;
            }
        };
        
        if (avformat_open_input(&request_ff->format_context_, request_ff->url_.c_str(), nullptr, nullptr) < 0) {
            LOG_ERROR("Error while opening AVFormat Input!");
            avformat_free_context(request_ff->format_context_);

            request_ff->format_context_ = nullptr;
            do_free_custom(request_ff);

            return false;
        }

        if (!open_ffmpeg_stream(*request_ff)) {
            request_ff->format_context_ = nullptr;
            do_free_custom(request_ff);

            return false;
        }
    }

    player_ffmpeg::player_ffmpeg(audio_driver *driver)
        : player_shared(driver) {
    }

    player_ffmpeg::~player_ffmpeg() {
    }
}