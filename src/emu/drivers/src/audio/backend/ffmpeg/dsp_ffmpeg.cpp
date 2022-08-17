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

#include <drivers/audio/backend/ffmpeg/dsp_ffmpeg.h>
#include <map>

#include <common/algorithm.h>
#include <common/log.h>

extern "C" {
#include <libswresample/swresample.h>
}

namespace eka2l1::drivers {
    static constexpr std::uint32_t CUSTOM_IO_BUFFER_SIZE = 8192;

    static std::map<four_cc, AVCodecID> FOUR_CC_TO_FFMPEG_CODEC_MAP = {
        { AMR_FOUR_CC_CODE, AV_CODEC_ID_AMR_NB },
        { MP3_FOUR_CC_CODE, AV_CODEC_ID_MP3 },
        { PCM16_FOUR_CC_CODE, AV_CODEC_ID_PCM_S16LE },
        { PCM8_FOUR_CC_CODE, AV_CODEC_ID_PCM_S8 }
    };

    static std::map<four_cc, const char*> FOUR_CC_TO_FILENAME_QUICK_REG_MAP = {
        { AMR_FOUR_CC_CODE, "sample.amr" },
        { MP3_FOUR_CC_CODE, "sample.mp3" },
    };

    dsp_output_stream_ffmpeg::dsp_output_stream_ffmpeg(drivers::audio_driver *aud)
        : dsp_output_stream_shared(aud)
        , codec_(nullptr)
        , av_format_(nullptr)
        , io_(nullptr)
        , custom_io_buffer_(nullptr)
        , timestamp_in_base_(0)
        , state_(STATE_NONE) {
        format(PCM16_FOUR_CC_CODE);
    }

    dsp_output_stream_ffmpeg::~dsp_output_stream_ffmpeg() {
        if (codec_) {
            avcodec_close(codec_);
            avcodec_free_context(&codec_);
        }

        if (av_format_) {
            avformat_close_input(&av_format_);
            avformat_free_context(av_format_);
        }

        if (io_) {
            av_freep(&io_->buffer);
            avio_context_free(&io_);
        }
    }

    int dsp_output_stream_ffmpeg::read_queued_data(std::uint8_t *buffer, int buffer_size) {
        if (queued_data_.empty()) {
            return AVERROR_EOF;
        }

        int read_size = common::min<int>(buffer_size, static_cast<int>(queued_data_.size()));
        std::memcpy(buffer, queued_data_.data(), read_size);

        if (read_size == queued_data_.size()) {
            queued_data_.clear();
        } else {
            queued_data_.erase(queued_data_.begin(), queued_data_.begin() + read_size);
        }

        return (read_size <= 0) ? AVERROR_EOF : read_size;
    }

    void dsp_output_stream_ffmpeg::get_supported_formats(std::vector<four_cc> &cc_list) {
        for (auto &map_pair : FOUR_CC_TO_FFMPEG_CODEC_MAP) {
            cc_list.push_back(map_pair.first);
        }
    }

    bool dsp_output_stream_ffmpeg::format(const four_cc fmt) {
        if ((fmt == PCM16_FOUR_CC_CODE) || (fmt == PCM8_FOUR_CC_CODE)) {
            if (codec_) {
                avcodec_close(codec_);
                avcodec_free_context(&codec_);

                codec_ = nullptr;
            }

            format_ = fmt;
            return true;
        }

        auto find_result = FOUR_CC_TO_FFMPEG_CODEC_MAP.find(fmt);

        if (find_result == FOUR_CC_TO_FFMPEG_CODEC_MAP.end()) {
            LOG_ERROR(DRIVER_AUD, "Four CC format not supported!");
            return false;
        }

        if (av_format_) {
            avformat_close_input(&av_format_);
            avformat_free_context(av_format_);
            av_format_ = nullptr;
        }

        if (io_) {
            av_freep(&io_->buffer);
            avio_context_free(&io_);

            io_ = nullptr;
        }

        custom_io_buffer_ = reinterpret_cast<std::uint8_t *>(av_malloc(CUSTOM_IO_BUFFER_SIZE));

        if (!custom_io_buffer_) {
            LOG_ERROR(DRIVER_AUD, "Can't allocate custom IO buffer!");
            return false;
        }

        io_ = avio_alloc_context(custom_io_buffer_, CUSTOM_IO_BUFFER_SIZE, 0, this, [](void *data, std::uint8_t *buf, int buf_size) {
            dsp_output_stream_ffmpeg *me = reinterpret_cast<dsp_output_stream_ffmpeg*>(data);
            return me->read_queued_data(buf, buf_size);
        }, nullptr, nullptr);

        if (!io_) {
            LOG_ERROR(DRIVER_AUD, "Can't initialize DSP custom IO!");
        }

        const AVCodec *decoder = avcodec_find_decoder(find_result->second);

        if (!decoder) {
            LOG_ERROR(DRIVER_AUD, "Decoder not supported by ffmpeg!");
            return false;
        }

        if (codec_) {
            avcodec_close(codec_);
            avcodec_free_context(&codec_);

            codec_ = nullptr;
        }

        codec_ = avcodec_alloc_context3(decoder);

        if (!codec_) {
            LOG_ERROR(DRIVER_AUD, "Can't alloc decode context!");
            return false;
        }

        codec_->channels = channels_;

        if (avcodec_open2(codec_, decoder, nullptr) < 0) {
            LOG_ERROR(DRIVER_AUD, "Can't open new context with codec");
            avcodec_free_context(&codec_);

            return false;
        }

        format_ = fmt;
        state_ = STATE_NONE;

        return true;
    }

    void dsp_output_stream_ffmpeg::queue_data_decode(const std::uint8_t *original, const std::size_t original_size) {
        const std::lock_guard<std::mutex> guard(decode_lock_);

        const bool all_zero = std::all_of(original, original + original_size, [](std::uint8_t i) { return i==0; });
        if (all_zero) {
            // Queue buffer
            buffer_.push(reinterpret_cast<const std::uint16_t*>(original), (original_size + 1) / 2);
            return;
        }

        queued_data_.resize(queued_data_.size() + original_size);
        std::memcpy(queued_data_.data() + queued_data_.size() - original_size, original, original_size);
    }

    bool dsp_output_stream_ffmpeg::decode_data(std::vector<std::uint8_t> &dest) {
        dest.clear();

        if (queued_data_.empty()) {
            // Nothing more to resolve
            return false;
        }

        if (!av_format_ || (state_ == STATE_NONE)) {
            const char *sample_filename = FOUR_CC_TO_FILENAME_QUICK_REG_MAP[format_];
            if (!sample_filename) {
                LOG_WARN(DRIVER_AUD, "No sample filename for quick recognization");
                sample_filename = "sample.raw";
            }

            av_format_ = avformat_alloc_context();
            if (!av_format_) {
                LOG_ERROR(DRIVER_AUD, "Can't initialize FFMPEG DSP format context!");
                return false;
            }

            av_format_->pb = io_;
            av_format_->flags |= AVFMT_FLAG_CUSTOM_IO;

            const AVInputFormat *format_target = nullptr;
            switch (format_) {
            case AMR_FOUR_CC_CODE:
                format_target = av_find_input_format("amrnb");
                break;

            case MP3_FOUR_CC_CODE:
                format_target = av_find_input_format("mp3");
                break;

            default:
                break;
            }

            if (avformat_open_input(&av_format_, sample_filename, format_target, nullptr) < 0) {
                LOG_ERROR(DRIVER_AUD, "This attempt fail to open FFMPEG DSP input, retry later");
                avformat_free_context(av_format_);

                av_format_ = nullptr;

                return false;
            }

            state_ = STATE_FORMAT_OPENED;
        }

        if (state_ == STATE_FORMAT_OPENED) {
            if (avformat_find_stream_info(av_format_, nullptr) < 0) {
                LOG_ERROR(DRIVER_AUD, "This attempt fail to find FFMPEG DSP input info, retry later");
                return false;
            }

            state_ = STATE_FRAME_READING;
        }

        if (state_ != STATE_FRAME_READING) {
            return false;
        }

        AVPacket *packet = av_packet_alloc();
        if (av_read_frame(av_format_, packet) < 0) {
            av_packet_free(&packet);
            return false;
        }
        
        int err = avcodec_send_packet(codec_, packet);
        av_packet_free(&packet);

        if (err >= 0) {
            AVFrame *frame = av_frame_alloc();

            if (avcodec_receive_frame(codec_, frame) < 0) {
                av_frame_free(&frame);
                return false;
            }

            dest.resize(channels_ * frame->nb_samples * sizeof(std::uint16_t));
            timestamp_in_base_ = frame->best_effort_timestamp;

            if ((channels_ != codec_->channels) || (frame->format != AV_SAMPLE_FMT_S16)) {
                SwrContext *swr = swr_alloc_set_opts(nullptr,
                    (channels_ == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, freq_,
                    frame->channel_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                    0, nullptr);

                if (swr_init(swr) < 0) {
                    LOG_ERROR(DRIVER_AUD, "Error initializing SWR context");
                    av_frame_free(&frame);

                    return false;
                }

                std::uint8_t *output = &dest[0];
                const std::uint8_t **input = const_cast<const std::uint8_t**>(frame->extended_data);

                const int result = swr_convert(swr, &output, frame->nb_samples, input, frame->nb_samples);
                swr_free(&swr);

                if (result < 0) {
                    LOG_ERROR(DRIVER_AUD, "Error resample audio data!");
                    return false;
                }
            } else {
                std::memcpy(&dest[0], frame->data[0], dest.size());
            }

            av_frame_free(&frame);
        }

        return true;
    }

    bool dsp_output_stream_ffmpeg::internal_decode_running_out() {
        return ((format_ != drivers::PCM16_FOUR_CC_CODE) && (queued_data_.size() <= CUSTOM_IO_BUFFER_SIZE * 2)) ||
            dsp_output_stream_shared::internal_decode_running_out();
    }
}