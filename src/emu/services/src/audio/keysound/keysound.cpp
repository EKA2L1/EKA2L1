/*
 * Copyright (c) 2019 EKA2L1 Team
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
#include <common/chunkyseri.h>
#include <drivers/audio/audio.h>

#include <services/audio/keysound/keysound.h>
#include <services/audio/keysound/ops.h>
#include <services/audio/keysound/ringtab.h>
#include <utils/err.h>

#include <epoc/epoc.h>
#include <kernel/process.h>

#include <cmath>

namespace eka2l1 {
    // sf_mw_classicui document
    // Reference from GUID-1A9B515C-C20F-4EC7-B62A-223B219BBC4E, Belle devlib
    // Implementation based of original S^3 open source code and documentation.
    keysound_session::keysound_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version) {
        drivers::audio_driver *aud_driver = svr->get_system()->get_audio_driver();

        if (aud_driver) {
            aud_out_ = aud_driver->new_output_stream(aud_driver->native_sample_rate(), 2,
                [this](std::int16_t *dest, std::size_t frames) {
                    return play_sounds(dest, frames);
                });
        }

        state_.target_freq_ = aud_driver->native_sample_rate();
    }

    keysound_session::parser_state::parser_state()
        : frames_(0)
        , target_freq_(44100)
        , frequency_(1)
        , ms_(0)
        , duration_unit_(0)
        , parser_pos_(0) {
    }

    std::size_t keysound_session::play_sounds(std::int16_t *buffer, std::size_t frames) {
        auto parse_to_get_freq = [this]() -> bool {
            if (state_.sound_.type_ == epoc::keysound::sound_type::sound_type_tone) {
                if (!state_.frames_) {
                    state_.frequency_ = state_.sound_.freq_;
                    state_.ms_ = state_.sound_.duration_;

                    state_.frames_ = 0;
                    state_.ms_ = 0;

                    return true;
                }

                state_.frames_ = 0;
                state_.ms_ = 0;

                return false;
            }

            state_.frames_ = 0;
            state_.ms_ = 0;

            // New parse
            while (state_.parser_pos_ < state_.sound_.sequences_.size()) {
                switch (state_.sound_.sequences_[state_.parser_pos_]) {
                case 0: {
                    break;
                }

                case 11: {
                    state_.parser_pos_ = state_.sound_.sequences_.size();
                    return false;
                }

                case 17: {
                    // Duration unit
                    state_.parser_pos_++;
                    state_.duration_unit_ = state_.sound_.sequences_[state_.parser_pos_];

                    break;
                }

                default: {
                    if (state_.sound_.sequences_[state_.parser_pos_] >= 0x40) {
                        const std::uint8_t frequency_index = state_.sound_.sequences_[state_.parser_pos_++];
                        const std::uint8_t duration_count = state_.sound_.sequences_[state_.parser_pos_++];

                        state_.ms_ = duration_count * state_.duration_unit_;
                        state_.frequency_ = epoc::keysound::frequency_map[static_cast<epoc::keysound::ring_frequency>(
                            frequency_index)];

                        return true;
                    }

                    break;
                }
                }

                state_.parser_pos_++;
            }

            return false;
        };

        const float amplitude = 0.8f;

        // Generate samples
        for (std::size_t t = 0; t < frames; t++) {
            if ((static_cast<double>(state_.frames_) / static_cast<double>(state_.target_freq_) * 1000.0) >= static_cast<double>(state_.ms_)) {
                if (!parse_to_get_freq()) {
                    aud_out_->stop();
                    return t;
                }
            }

            const double sample = amplitude * std::sin(2 * 3.14 * (static_cast<double>(state_.frequency_) / static_cast<double>(state_.target_freq_)) * (state_.frames_ + 1));
            const std::int16_t sample_i16 = static_cast<std::int16_t>(sample * 0x7FFF);

            buffer[2 * t] = sample_i16;
            buffer[2 * t + 1] = sample_i16;

            state_.frames_++;
        }

        return frames;
    }

    void keysound_session::init(service::ipc_context *ctx) {
        const bool inited = server<keysound_server>()->initialized();
        ctx->write_data_to_descriptor_argument<std::int32_t>(0, inited);

        if (!inited) {
            server<keysound_server>()->initialized(true);
        }

        // Store app UID
        app_uid_ = *ctx->get_argument_value<std::uint32_t>(1);
        ctx->complete(epoc::error_none);
    }

    void keysound_session::push_context(service::ipc_context *ctx) {
        const auto item_count = ctx->get_argument_value<std::uint32_t>(0);
        const auto uid = ctx->get_argument_value<std::uint32_t>(2);
        const auto rsc_id = ctx->get_argument_value<std::int32_t>(3);

        std::uint8_t *item_def_ptr = ctx->get_descriptor_argument_ptr(1);

        if (!item_count || !uid || !rsc_id || !item_def_ptr) {
            ctx->complete(epoc::error_argument);
            return;
        }

        static constexpr std::uint32_t SIZE_EACH_ITEM_DEF = 5;

        // Push the context
        common::chunkyseri seri(item_def_ptr, item_count.value() * SIZE_EACH_ITEM_DEF,
            common::SERI_MODE_READ);

        contexts_.emplace_back(seri, uid.value(), rsc_id.value(), item_count.value());
        ctx->complete(epoc::error_none);
    }

    void keysound_session::pop_context(service::ipc_context *ctx) {
        contexts_.pop_back();
        ctx->complete(epoc::error_none);
    }

    void keysound_session::play_sid(service::ipc_context *ctx) {
        LOG_TRACE("Trying to play sound with ID 0x{:x}, stubbed", ctx->get_argument_value<std::uint32_t>(0).value());
        epoc::keysound::sound_info *info = server<keysound_server>()->get_sound(ctx->get_argument_value<std::uint32_t>(0).value());

        if (!info) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        if (info->type_ == epoc::keysound::sound_type::sound_type_file) {
            LOG_WARN("Sound type file unsupported, skip. Please revisit");
            ctx->complete(epoc::error_none);
            return;
        }

        if (aud_out_->is_playing()) {
            aud_out_->stop();
        }

        state_.sound_ = *info;
        aud_out_->start();

        ctx->complete(epoc::error_none);
    }

    void keysound_session::add_sids(service::ipc_context *ctx) {
        std::uint8_t *sound_descriptor = ctx->get_descriptor_argument_ptr(2);
        const std::optional<std::uint32_t> sound_descriptor_size = ctx->get_argument_value<std::uint32_t>(1);
        const std::optional<std::uint32_t> uid = ctx->get_argument_value<std::uint32_t>(0);

        if (!sound_descriptor || !sound_descriptor_size || !uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        common::ro_buf_stream stream(sound_descriptor, sound_descriptor_size.value());

        std::uint16_t count = 0;
        stream.read(&count, 2);

        epoc::keysound::sound_info info;

        for (std::uint16_t i = 0; i < count; i++) {
            stream.read(&info.id_, 4);
            stream.read(&info.priority_, 2);
            stream.read(&info.preference_, 4);

            std::uint8_t type;
            stream.read(&type, 1);

            switch (type) {
            case 0: {
                info.type_ = epoc::keysound::sound_type::sound_type_file;

                // Filename
                LOG_TRACE("Load filename sound TODO. Please notice this");
                break;
            }

            case 1: {
                info.type_ = epoc::keysound::sound_type::sound_type_tone;

                // Simple tone
                std::uint16_t freq = 0;
                std::uint32_t duration = 0;

                stream.read(&info.freq_, 2);
                stream.read(&info.duration_, 4);

                break;
            }

            case 2: {
                info.type_ = epoc::keysound::sound_type::sound_type_sequence;

                std::uint16_t sequence_length = 0;
                stream.read(&sequence_length, 2);

                info.sequences_.resize(sequence_length);
                stream.read(&(info.sequences_[0]), sequence_length);

                break;
            }

            default:
                break;
            }

            stream.read(&info.volume_, 1);
            server<keysound_server>()->add_sound(info);
        }

        ctx->complete(epoc::error_none);
    }

    void keysound_session::bring_to_foreground(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void keysound_session::lock_context(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void keysound_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::keysound::opcode_init: {
            init(ctx);
            break;
        }

        case epoc::keysound::opcode_push_context: {
            push_context(ctx);
            break;
        }

        case epoc::keysound::opcode_pop_context: {
            pop_context(ctx);
            break;
        }

        case epoc::keysound::opcode_add_sids: {
            add_sids(ctx);
            break;
        }

        case epoc::keysound::opcode_play_sid: {
            play_sid(ctx);
            break;
        }

        case epoc::keysound::opcode_bring_to_foreground: {
            bring_to_foreground(ctx);
            break;
        }

        case epoc::keysound::opcode_lock_context: {
            lock_context(ctx);
            break;
        }

        default:
            LOG_ERROR("Unimplemented keysound server opcode: {}", ctx->msg->function);
            break;
        }
    }

    static constexpr const char *KEYSOUND_SERVER_NAME = "KeySoundServer";

    keysound_server::keysound_server(system *sys)
        : service::typical_server(sys, KEYSOUND_SERVER_NAME)
        , inited_(false) {
    }

    void keysound_server::connect(service::ipc_context &context) {
        create_session<keysound_session>(&context);
        context.complete(0);
    }

    epoc::keysound::sound_info *keysound_server::get_sound(const std::uint32_t sid) {
        auto ite = std::find_if(sounds_.begin(), sounds_.end(), [sid](const epoc::keysound::sound_info &info) {
            return info.id_ == sid;
        });

        if (ite == sounds_.end()) {
            return nullptr;
        }

        return &(*ite);
    }
}