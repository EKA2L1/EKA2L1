/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <drivers/audio/clip_writer.h>

#include <common/fileutils.h>
#include <common/log.h>

#include <algorithm>
#include <array>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace eka2l1::drivers {
    struct clip_codec {
        std::uint32_t four_cc;
        const char *encoder_name;
        const char *muxer_name;
        clip_record_format default_format;
        std::vector<std::uint32_t> sample_rates;
        std::vector<std::uint32_t> channel_counts;
        std::vector<std::uint32_t> bit_rates;

        // Whether new packets can follow the existing stream, which the encoder may also configure from.
        bool (*can_extend)(const AVCodecParameters *params, clip_record_format &format);

        // The bit rate a packet was coded at, or 0 if the stream does not say.
        std::uint32_t (*packet_bit_rate)(const AVPacket *packet);
    };

    static const std::uint32_t AMR_MODE_BIT_RATES[8] = { 4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200 };

    static const clip_codec AMR_NB_CODEC = {
        AUDIO_AMR_CODEC_4CC,
        "libopencore_amrnb",
        "amr",
        { 8000, 1, 12200 },
        { 8000 },
        { 1 },
        std::vector<std::uint32_t>(AMR_MODE_BIT_RATES, AMR_MODE_BIT_RATES + 8),
        [](const AVCodecParameters *params, clip_record_format &) {
            return params->codec_id == AV_CODEC_ID_AMR_NB;
        },
        [](const AVPacket *packet) -> std::uint32_t {
            // RFC 4867 storage format: the frame type is in the header byte; types 8 and up carry no speech.
            const int type = (packet->size > 0) ? ((packet->data[0] >> 3) & 0xF) : 8;
            return (type < 8) ? AMR_MODE_BIT_RATES[type] : 0;
        }
    };

    static const clip_codec AAC_LC_CODEC = {
        AUDIO_AAC_CODEC_4CC,
        "aac",
        "mp4",
        { 48000, 1, 64000 },
        { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 },
        { 1, 2 },
        { 16000, 24000, 32000, 48000, 64000, 96000, 128000, 160000, 192000, 256000 },
        [](const AVCodecParameters *params, clip_record_format &format) {
            // The new packets come from an AAC-LC encoder (object type 2 in AudioSpecificConfig).
            if ((params->codec_id != AV_CODEC_ID_AAC) || (params->extradata_size < 2) || ((params->extradata[0] >> 3) != 2) ||
                (params->sample_rate <= 0) || (params->ch_layout.nb_channels < 1) || (params->ch_layout.nb_channels > 2)) {
                return false;
            }

            format.sample_rate = params->sample_rate;
            format.channels = params->ch_layout.nb_channels;

            if (params->bit_rate > 0) {
                format.bit_rate = static_cast<std::uint32_t>(params->bit_rate);
            }

            return true;
        },
        [](const AVPacket *) -> std::uint32_t {
            return 0;
        }
    };

    // Encodes through FFmpeg into a container that keeps no appendable layout, so appending rewrites
    // the file: the existing packets are copied over as they are, followed by newly encoded ones.
    class clip_writer_ffmpeg : public clip_writer {
        struct stored_packet {
            std::vector<std::uint8_t> data;
            std::int64_t pts;
            std::int64_t duration;
        };

        const clip_codec &codec_;

        // Loaded only while a take is being recorded. Timestamps are in samples.
        std::vector<stored_packet> existing_packets_;

        AVFormatContext *output_ = nullptr;
        AVStream *stream_ = nullptr;
        AVCodecContext *encoder_ = nullptr;
        AVFrame *frame_ = nullptr;
        AVPacket *packet_ = nullptr;

        // Interleaved samples not yet making up a whole encoder frame.
        std::vector<std::int16_t> pending_;
        std::int64_t next_pts_ = 0;
        std::int64_t end_pts_ = 0;

        void release() {
            existing_packets_.clear();
            existing_packets_.shrink_to_fit();

            av_packet_free(&packet_);
            av_frame_free(&frame_);
            avcodec_free_context(&encoder_);

            if (output_) {
                if (output_->pb) {
                    avio_closep(&output_->pb);
                }

                avformat_free_context(output_);
                output_ = nullptr;
            }

            stream_ = nullptr;
        }

        bool write_packet(AVPacket *packet) {
            end_pts_ = std::max(end_pts_, packet->pts + packet->duration);

            packet->stream_index = stream_->index;
            av_packet_rescale_ts(packet, AVRational{ 1, static_cast<int>(format_.sample_rate) }, stream_->time_base);

            return av_interleaved_write_frame(output_, packet) >= 0;
        }

        bool drain_encoder() {
            int result = 0;

            while ((result = avcodec_receive_packet(encoder_, packet_)) >= 0) {
                if (!write_packet(packet_)) {
                    return false;
                }
            }

            return (result == AVERROR(EAGAIN)) || (result == AVERROR_EOF);
        }

        bool encode_pending(const std::size_t frames) {
            // av_frame_unref() after the previous frame cleared these.
            frame_->format = encoder_->sample_fmt;
            frame_->sample_rate = encoder_->sample_rate;
            frame_->nb_samples = static_cast<int>(frames);

            if ((av_channel_layout_copy(&frame_->ch_layout, &encoder_->ch_layout) < 0) || (av_frame_get_buffer(frame_, 0) < 0)) {
                return false;
            }

            const std::size_t channels = format_.channels;

            if (encoder_->sample_fmt == AV_SAMPLE_FMT_S16) {
                std::copy(pending_.begin(), pending_.begin() + frames * channels, reinterpret_cast<std::int16_t *>(frame_->data[0]));
            } else {
                for (std::size_t channel = 0; channel < channels; channel++) {
                    float *output = reinterpret_cast<float *>(frame_->data[channel]);

                    for (std::size_t i = 0; i < frames; i++) {
                        output[i] = pending_[i * channels + channel] / 32768.0f;
                    }
                }
            }

            pending_.erase(pending_.begin(), pending_.begin() + frames * channels);

            frame_->pts = next_pts_;
            next_pts_ += frames;

            const bool sent = avcodec_send_frame(encoder_, frame_) >= 0;
            av_frame_unref(frame_);

            return sent && drain_encoder();
        }

    public:
        explicit clip_writer_ffmpeg(const std::string &path, const clip_codec &codec)
            : clip_writer(path)
            , codec_(codec) {
            format_ = codec_.default_format;
        }

        ~clip_writer_ffmpeg() override {
            end();
        }

        // Reads the clip's stream, keeping its packets if asked. Returns false if new packets cannot follow it.
        bool scan(const bool keep_packets) {
            existing_packets_.clear();
            has_existing_audio_ = false;
            existing_frames_ = 0;
            end_pts_ = 0;

            std::FILE *file = common::open_c_file(path_, "rb");
            if (!file) {
                return true;
            }

            const bool empty = (std::fseek(file, 0, SEEK_END) == 0) && (std::ftell(file) == 0);
            std::fclose(file);

            if (empty) {
                return true;
            }

            AVFormatContext *input = nullptr;
            if (avformat_open_input(&input, path_.c_str(), nullptr, nullptr) < 0) {
                return false;
            }

            const int stream_index = (avformat_find_stream_info(input, nullptr) >= 0)
                ? av_find_best_stream(input, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0)
                : -1;

            const bool usable = (stream_index >= 0) && codec_.can_extend(input->streams[stream_index]->codecpar, format_);

            if (usable) {
                const AVRational time_base = input->streams[stream_index]->time_base;
                const AVRational sample_base{ 1, static_cast<int>(format_.sample_rate) };

                AVPacket *packet = av_packet_alloc();

                while (packet && (av_read_frame(input, packet) >= 0)) {
                    if (packet->stream_index == stream_index) {
                        const std::int64_t pts = av_rescale_q(packet->pts, time_base, sample_base);
                        const std::int64_t duration = av_rescale_q(packet->duration, time_base, sample_base);

                        if (const std::uint32_t bit_rate = codec_.packet_bit_rate(packet)) {
                            format_.bit_rate = bit_rate;
                        }

                        if (keep_packets) {
                            existing_packets_.push_back({ std::vector<std::uint8_t>(packet->data, packet->data + packet->size), pts, duration });
                        }

                        end_pts_ = std::max(end_pts_, pts + duration);
                    }

                    av_packet_unref(packet);
                }

                av_packet_free(&packet);

                has_existing_audio_ = true;
                existing_frames_ = static_cast<std::uint64_t>(std::max<std::int64_t>(end_pts_, 0));
            }

            avformat_close_input(&input);
            return usable;
        }

        bool probe() {
            return scan(false);
        }

        std::uint32_t codec_four_cc() const override {
            return codec_.four_cc;
        }

        std::vector<std::uint32_t> supported_sample_rates() const override {
            if (has_existing_audio_) {
                return { format_.sample_rate };
            }

            return codec_.sample_rates;
        }

        std::vector<std::uint32_t> supported_channel_counts() const override {
            if (has_existing_audio_) {
                return { format_.channels };
            }

            return codec_.channel_counts;
        }

        std::vector<std::uint32_t> supported_bit_rates() const override {
            return codec_.bit_rates;
        }

        bool begin() override {
            if (output_) {
                return false;
            }

            // The clip may have been rewritten since it was probed.
            const bool had_audio = has_existing_audio_;
            const std::uint64_t probed_frames = existing_frames_;
            const clip_record_format requested = format_;

            if (!scan(true) || (has_existing_audio_ != had_audio) || (existing_frames_ != probed_frames)) {
                existing_packets_.clear();
                return false;
            }

            // Only the bit rate may change between takes.
            format_.bit_rate = requested.bit_rate;

            const AVCodec *codec = avcodec_find_encoder_by_name(codec_.encoder_name);

            if (!codec || (avformat_alloc_output_context2(&output_, nullptr, codec_.muxer_name, path_.c_str()) < 0)) {
                LOG_ERROR(DRIVER_AUD, "Unable to set up {} recording into {}", codec_.encoder_name, path_);
                return false;
            }

            encoder_ = avcodec_alloc_context3(codec);
            frame_ = av_frame_alloc();
            packet_ = av_packet_alloc();
            stream_ = avformat_new_stream(output_, nullptr);

            if (!encoder_ || !frame_ || !packet_ || !stream_) {
                release();
                return false;
            }

            const AVSampleFormat *sample_formats = nullptr;
            int sample_format_count = 0;

            if ((avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                     reinterpret_cast<const void **>(&sample_formats), &sample_format_count) < 0) || (sample_format_count == 0) ||
                ((sample_formats[0] != AV_SAMPLE_FMT_S16) && (sample_formats[0] != AV_SAMPLE_FMT_FLTP))) {
                release();
                return false;
            }

            encoder_->sample_fmt = sample_formats[0];
            encoder_->sample_rate = static_cast<int>(format_.sample_rate);
            encoder_->bit_rate = format_.bit_rate;
            encoder_->time_base = AVRational{ 1, static_cast<int>(format_.sample_rate) };
            av_channel_layout_default(&encoder_->ch_layout, static_cast<int>(format_.channels));

            if (codec->id == AV_CODEC_ID_AAC) {
                encoder_->profile = AV_PROFILE_AAC_LOW;
            }

            if (output_->oformat->flags & AVFMT_GLOBALHEADER) {
                encoder_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            if ((avcodec_open2(encoder_, codec, nullptr) < 0) || (avcodec_parameters_from_context(stream_->codecpar, encoder_) < 0)) {
                LOG_ERROR(DRIVER_AUD, "Unable to open the {} encoder at {} Hz, {} channels, {} bps", codec_.encoder_name,
                    format_.sample_rate, format_.channels, format_.bit_rate);
                release();

                return false;
            }

            stream_->time_base = encoder_->time_base;

            if ((avio_open(&output_->pb, path_.c_str(), AVIO_FLAG_WRITE) < 0) || (avformat_write_header(output_, nullptr) < 0)) {
                LOG_ERROR(DRIVER_AUD, "Unable to write the {} header of {}", codec_.muxer_name, path_);
                release();

                return false;
            }

            for (stored_packet &stored : existing_packets_) {
                packet_->data = stored.data.data();
                packet_->size = static_cast<int>(stored.data.size());
                packet_->pts = packet_->dts = stored.pts;
                packet_->duration = stored.duration;
                packet_->flags = AV_PKT_FLAG_KEY;

                // Muxing takes ownership of the packet's fields but not of borrowed data.
                const bool written = write_packet(packet_);
                av_packet_unref(packet_);

                if (!written) {
                    release();
                    return false;
                }
            }

            // The encoder delays its output by the priming samples; start past them so the new packets
            // follow the existing ones.
            next_pts_ = existing_packets_.empty() ? 0 : end_pts_ + encoder_->initial_padding;
            pending_.clear();

            existing_packets_.clear();
            existing_packets_.shrink_to_fit();

            return true;
        }

        bool write(const std::int16_t *samples, const std::size_t frames) override {
            if (!output_) {
                return false;
            }

            pending_.insert(pending_.end(), samples, samples + frames * format_.channels);
            written_frames_ += frames;

            const std::size_t frame_size = static_cast<std::size_t>(encoder_->frame_size);

            while (pending_.size() >= frame_size * format_.channels) {
                if (!encode_pending(frame_size)) {
                    return false;
                }
            }

            avio_flush(output_->pb);
            return true;
        }

        bool end() override {
            if (!output_) {
                return false;
            }

            bool written = pending_.empty() || encode_pending(pending_.size() / format_.channels);
            written = written && (avcodec_send_frame(encoder_, nullptr) >= 0) && drain_encoder();
            written = (av_write_trailer(output_) >= 0) && written;

            release();

            // Reload what was muxed, so the next take appends to it.
            written_frames_ = 0;
            return probe() && written;
        }
    };

    static std::unique_ptr<clip_writer> new_clip_writer_ffmpeg(const std::string &path, const clip_codec &codec) {
        auto writer = std::make_unique<clip_writer_ffmpeg>(path, codec);
        return writer->probe() ? std::move(writer) : nullptr;
    }

    std::unique_ptr<clip_writer> new_clip_writer_amr(const std::string &path) {
        return new_clip_writer_ffmpeg(path, AMR_NB_CODEC);
    }

    std::unique_ptr<clip_writer> new_clip_writer_aac_mp4(const std::string &path) {
        return new_clip_writer_ffmpeg(path, AAC_LC_CODEC);
    }
}
