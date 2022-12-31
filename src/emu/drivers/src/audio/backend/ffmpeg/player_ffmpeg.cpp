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
#include <common/time.h>

#include <drivers/audio/backend/ffmpeg/player_ffmpeg.h>

extern "C" {
#include <libswresample/swresample.h>
}

namespace eka2l1::drivers {
    void player_ffmpeg::deinit() {
        if (format_context_) {
            avformat_close_input(&format_context_);
            avformat_free_context(format_context_);
        }

        if (custom_io_buffer_) {
            av_freep(&custom_io_buffer_);
        }

        if (custom_io_) {
            avio_context_free(&custom_io_);
        }

        if (codec_) {
            avcodec_free_context(&codec_);
        }
    }

    bool player_ffmpeg::open_ffmpeg_stream() {
        if (avformat_find_stream_info(format_context_, nullptr) < 0) {
            LOG_ERROR(DRIVER_AUD, "Error while finding stream info of input {}", url_);
            avformat_free_context(format_context_);
            format_context_ = nullptr;

            return false;
        }

        int best_stream_index = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

        if (best_stream_index < 0) {
            LOG_ERROR(DRIVER_AUD, "Unable to retrieve best audio stream!");
            return false;
        }

        AVStream *stream = format_context_->streams[best_stream_index];
        const AVCodec *nice_codec = avcodec_find_decoder(stream->codecpar->codec_id);

        if (!nice_codec) {
            LOG_ERROR(DRIVER_AUD, "Unable to get stream codec!");
            return false;
        }

        // Trying to open decoder now
        if (!codec_) {
            codec_ = avcodec_alloc_context3(nice_codec);
        }

        if (avcodec_parameters_to_context(codec_, format_context_->streams[best_stream_index]->codecpar) < 0) {
            LOG_ERROR(DRIVER_AUD, "Fail to set stream parameters to codec!");
            return false;
        }

        if (avcodec_open2(codec_, nice_codec, nullptr) < 0) {
            LOG_ERROR(DRIVER_AUD, "Unable to open codec of stream url {}", url_);

            avformat_free_context(format_context_);
            format_context_ = nullptr;

            return false;
        }

        channels_ = codec_->channels;
        freq_ = codec_->sample_rate;

        const double time_base = av_q2d(stream->time_base);
        duration_us_ = static_cast<std::uint64_t>(static_cast<double>(stream->duration) * time_base * common::microsecs_per_sec);

        return true;
    }

    bool player_ffmpeg::open_url(const std::string &url) {
        const std::lock_guard<std::mutex> guard(lock_);
        flags_ |= 1;

        if (output_stream_ && output_stream_->is_playing()) {
            output_stream_->stop();
        }

        flags_ &= ~1;

        deinit();
        url_ = url;

        return make_backend_source();
    }

    void player_ffmpeg::reset_request() {
        if (!format_context_) {
            if (!make_backend_source()) {
                return;
            }
        }

        set_position_for_custom_format(0);
    }

    bool player_ffmpeg::is_ready_to_play() {
        return format_context_;
    }

    void player_ffmpeg::get_more_data() {
        // Data drain, try to get more
        if (flags_ & 1) {
            output_stream_->stop();
            return;
        }

        if (av_read_frame(format_context_, &packet_) < 0) {
            flags_ |= 1;
            return;
        }

        // Send packet to decoder
        if (avcodec_send_packet(codec_, &packet_) >= 0) {
            AVFrame *frame = av_frame_alloc();
            int err = avcodec_receive_frame(codec_, frame);

            if (err < 0) {
                LOG_ERROR(DRIVER_AUD, "Error while decoding a frame err={}!", err);
                av_frame_free(&frame);
                return;
            }

            std::size_t base_ptr = 0;

            if (use_push_new_data_) {
                base_ptr = data_.size();
                use_push_new_data_ = false;
            }

            data_.resize(base_ptr + channels_ * frame->nb_samples * sizeof(std::int16_t));

            if ((frame->format != AV_SAMPLE_FMT_S16) || (frame->channels != channels_)
                || (frame->sample_rate != freq_)) {
                // Resample it
                const int dest_channel_type = (channels_ == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

                SwrContext *swr = swr_alloc_set_opts(nullptr,
                    dest_channel_type, AV_SAMPLE_FMT_S16, freq_,
                    frame->channel_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                    0, nullptr);

                if (swr_init(swr) < 0) {
                    LOG_ERROR(DRIVER_AUD, "Error initializing SWR context");
                    av_frame_free(&frame);

                    return;
                }

                std::uint8_t *output = data_.data() + base_ptr;
                const std::uint8_t **source = const_cast<const std::uint8_t**>(frame->extended_data);

                const int result = swr_convert(swr, &output, frame->nb_samples, source, frame->nb_samples);
                swr_free(&swr);

                if (result < 0) {
                    LOG_ERROR(DRIVER_AUD, "Error resample audio data!");
                    flags_ |= 1;
                    return;
                }
            } else {
                // Just gonna copy smh
                std::memcpy(&data_[base_ptr], frame->data[0], data_.size());
            }

            av_frame_free(&frame);
        }

        data_pointer_ = 0;
    }

    static int ffmpeg_custom_rw_io_read(void *opaque, std::uint8_t *buf, int buf_size) {
        common::rw_stream *stream = reinterpret_cast<common::rw_stream *>(opaque);
        const std::uint64_t size = stream->read(buf, buf_size);

        return ((size <= 0) ? AVERROR_EOF : static_cast<int>(size));
    }

    static int ffmpeg_custom_rw_io_write(void *opaque, std::uint8_t *buf, int buf_size) {
        common::rw_stream *stream = reinterpret_cast<common::rw_stream *>(opaque);
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

        common::rw_stream *stream = reinterpret_cast<common::rw_stream *>(opaque);
        stream->seek(offset, pos_seek_mode);

        return static_cast<std::int64_t>(stream->tell());
    }

    bool player_ffmpeg::open_custom(common::rw_stream *the_stream) {
        const std::lock_guard<std::mutex> guard(lock_);
        flags_ |= 1;

        if (output_stream_ && output_stream_->is_playing()) {
            output_stream_->stop();
        }

        flags_ &= ~1;
        deinit();

        static constexpr std::uint32_t CUSTOM_IO_BUFFER_SIZE = 8192;

        custom_io_buffer_ = reinterpret_cast<std::uint8_t *>(av_malloc(CUSTOM_IO_BUFFER_SIZE));

        if (!custom_io_buffer_) {
            return false;
        }

        custom_io_ = avio_alloc_context(custom_io_buffer_, CUSTOM_IO_BUFFER_SIZE,
            0, the_stream, ffmpeg_custom_rw_io_read, ffmpeg_custom_rw_io_write, ffmpeg_custom_rw_io_seek);

        if (!custom_io_) {
            av_freep(&custom_io_buffer_);
            return false;
        }

        return make_backend_source();
    }

    bool player_ffmpeg::set_position_for_custom_format(const std::uint64_t pos_in_us) {
        avformat_flush(format_context_);

        // The unit is microseconds
        if (avformat_seek_file(format_context_, -1, INT64_MIN, pos_in_us, INT64_MAX, 0) < 0) {
            LOG_ERROR(DRIVER_AUD, "Error seeking the stream!");
            return false;
        }

        return true;
    }

    bool player_ffmpeg::crop() {
        LOG_ERROR(DRIVER_AUD, "Crop audio operation unsupported in FFMPEG backend");
        return false;
    }

    bool player_ffmpeg::record() {
        LOG_ERROR(DRIVER_AUD, "Record audio operation unsupported in FFMPEG backend");
        return false;
    }

    bool player_ffmpeg::set_dest_freq(const std::uint32_t freq) {
        if (!output_encoder_) {
            LOG_ERROR(DRIVER_AUD, "No encoder is initialized!");
            return false;
        }

        const int *sample_rate_support_array = output_encoder_->supported_samplerates;
        while (*sample_rate_support_array) {
            if (*sample_rate_support_array == static_cast<std::int32_t>(freq)) {
                freq_ = static_cast<std::int32_t>(freq);
                return true;
            }
        }

        return false;
    }

    bool player_ffmpeg::set_dest_channel_count(const std::uint32_t cn) {
        if (!output_encoder_) {
            LOG_ERROR(DRIVER_AUD, "No encoder is initialized!");
            return false;
        }

        const std::uint64_t *layout_support_layout = output_encoder_->channel_layouts;
        while (*layout_support_layout) {
            if (av_get_channel_layout_nb_channels(*layout_support_layout) == static_cast<std::int32_t>(cn)) {
                channels_ = cn;
                channel_layout_dest_ = *layout_support_layout;

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
        const AVCodecID codec_id = four_cc_to_av_codec_id(enc);

        if (codec_id == AV_CODEC_ID_NONE) {
            return false;
        }

        const AVCodec *new_codec = avcodec_find_encoder(codec_id);
        if (!new_codec) {
            LOG_ERROR(DRIVER_AUD, "Unable to find new encoder for codec id {}", static_cast<int>(codec_id));
            return false;
        }

        if (!(new_codec->supported_samplerates) || !(new_codec->channel_layouts)) {
            // One of those arrays is empty. Return
            LOG_ERROR(DRIVER_AUD, "Supported sample rates or supported channel layouts array is empty!");
            return false;
        }

        channels_ = av_get_channel_layout_nb_channels(*new_codec->channel_layouts);
        freq_ = *new_codec->supported_samplerates;
        encoding_ = enc;
        output_encoder_ = new_codec;

        return true;
    }

    bool player_ffmpeg::make_backend_source() {
        if (format_context_) {
            avformat_free_context(format_context_);
        }

        format_context_ = avformat_alloc_context();

        if (custom_io_) {
            format_context_->pb = custom_io_;
            format_context_->flags |= AVFMT_FLAG_CUSTOM_IO;

            url_ = "Dummy";
        }

        auto do_free_custom = [&]() {
            if (custom_io_buffer_) {
                av_freep(&custom_io_buffer_);
            }

            if (custom_io_) {
                avio_context_free(&custom_io_);
            }
        };

        if (avformat_open_input(&format_context_, url_.c_str(), nullptr, nullptr) < 0) {
            LOG_ERROR(DRIVER_AUD, "Error while opening AVFormat Input!");
            avformat_free_context(format_context_);

            format_context_ = nullptr;
            do_free_custom();

            return false;
        }

        // The open input above already freed the custom IO buffer
        custom_io_buffer_ = nullptr;

        if (!open_ffmpeg_stream()) {
            format_context_ = nullptr;
            do_free_custom();
    
            return false;
        }

        return true;
    }

    player_ffmpeg::player_ffmpeg(audio_driver *driver)
        : player_shared(driver)
        , codec_(nullptr)
        , format_context_(nullptr)
        , output_encoder_(nullptr)
        , channel_layout_dest_(0)
        , custom_io_(nullptr)
        , custom_io_buffer_(nullptr)
        , duration_us_(0) {
        av_init_packet(&packet_);
    }

    player_ffmpeg::~player_ffmpeg() {
        deinit();
    }
}