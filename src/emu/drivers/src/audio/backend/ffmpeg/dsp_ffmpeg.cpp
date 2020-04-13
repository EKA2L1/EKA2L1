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

#include <common/log.h>

extern "C" {
#include <libswresample/swresample.h>
}

namespace eka2l1::drivers {
    static std::map<four_cc, AVCodecID> FOUR_CC_TO_FFMPEG_CODEC_MAP = {
        { AMR_FOUR_CC_CODE, AV_CODEC_ID_AMR_NB },
        { MP3_FOUR_CC_CODE, AV_CODEC_ID_MP3 },
        { PCM16_FOUR_CC_CODE, AV_CODEC_ID_PCM_S16LE },
        { PCM8_FOUR_CC_CODE, AV_CODEC_ID_PCM_S8 }
    };

    void dsp_output_stream_ffmpeg::get_supported_formats(std::vector<four_cc> &cc_list) {
        for (auto &map_pair : FOUR_CC_TO_FFMPEG_CODEC_MAP) {
            cc_list.push_back(map_pair.first);
        }
    }

    bool dsp_output_stream_ffmpeg::format(const four_cc fmt) {
        auto find_result = FOUR_CC_TO_FFMPEG_CODEC_MAP.find(fmt);

        if (find_result == FOUR_CC_TO_FFMPEG_CODEC_MAP.end()) {
            LOG_ERROR("Four CC format not supported!");
            return false;
        }

        AVCodec *decoder = avcodec_find_decoder(find_result->second);

        if (!decoder) {
            LOG_ERROR("Decoder not supported by ffmpeg!");
            return false;
        }

        codec_ = avcodec_alloc_context3(decoder);

        if (!codec_) {
            LOG_ERROR("Can't alloc decode context!");
            return false;
        }

        format_ = fmt;
        return true;
    }

    void dsp_output_stream_ffmpeg::decode_data(dsp_buffer &original, std::vector<std::uint8_t> &dest) {
        AVPacket packet;
        av_init_packet(&packet);

        packet.size = original.size();
        packet.data = original.data();

        if (avcodec_send_packet(codec_, &packet) >= 0) {
            AVFrame *frame = av_frame_alloc();

            if (avcodec_receive_frame(codec_, frame) < 0) {
                av_frame_free(&frame);
                return;
            }

            dest.resize(2 * frame->nb_samples * sizeof(std::uint16_t));

            if ((channels_ != codec_->channels) || (frame->format != AV_SAMPLE_FMT_S16)) {
                SwrContext *swr = swr_alloc_set_opts(nullptr,
                    AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, freq_,
                    frame->channel_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                    0, nullptr);

                if (swr_init(swr) < 0) {
                    LOG_ERROR("Error initializing SWR context");
                    av_frame_free(&frame);

                    return;
                }

                std::uint8_t *output = &dest[0];
                const std::uint8_t *source = frame->data[0];

                const int result = swr_convert(swr, &output, frame->nb_samples, &source, frame->nb_samples);
                swr_free(&swr);

                if (result < 0) {
                    LOG_ERROR("Error resample audio data!");
                    return;
                }
            } else {
                std::memcpy(&dest[0], frame->data[0], dest.size());
            }
        }
    }
}