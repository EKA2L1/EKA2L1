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

#include <services/audio/mmf/common.h>
#include <services/audio/mmf/dev.h>
#include <utils/err.h>

#include <system/epoc.h>
#include <kernel/kernel.h>

#include <common/algorithm.h>
#include <common/log.h>

namespace eka2l1 {
    static const char *MMF_DEV_SERVER_NAME = "!MMFDevServer";

    const std::uint32_t freq_enum_to_number(const epoc::mmf_sample_rate rate) {
        switch (rate) {
        case epoc::mmf_sample_rate_8000hz:
            return 8000;

        case epoc::mmf_sample_rate_11025hz:
            return 11025;

        case epoc::mmf_sample_rate_12000hz:
            return 12000;

        case epoc::mmf_sample_rate_16000hz:
            return 16000;

        case epoc::mmf_sample_rate_22050hz:
            return 22050;

        case epoc::mmf_sample_rate_24000hz:
            return 24000;

        case epoc::mmf_sample_rate_32000hz:
            return 32000;

        case epoc::mmf_sample_rate_44100hz:
            return 44100;

        case epoc::mmf_sample_rate_48000hz:
            return 48000;

        case epoc::mmf_sample_rate_64000hz:
            return 64000;

        case epoc::mmf_sample_rate_88200hz:
            return 88200;

        case epoc::mmf_sample_rate_96000hz:
            return 96000;

        default:
            break;
        }

        return 8000;
    }

    inline std::uint32_t duration_to_samples(const std::uint32_t duration_ms, const epoc::mmf_sample_rate rate) {
        static constexpr std::uint64_t MS_PER_SEC = 1000;
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(duration_ms * freq_enum_to_number(rate) / MS_PER_SEC));
    }

    inline std::uint32_t duration_to_bytes(const std::uint32_t duration_ms, const epoc::mmf_capabilities &caps) {
        return duration_to_samples(duration_ms, caps.rate_) * caps.average_bytes_per_sample();
    }

    std::uint32_t epoc::mmf_capabilities::average_bytes_per_sample() const {
        std::uint8_t bb = 0;

        switch (encoding_) {
        case epoc::mmf_encoding_8bit_pcm:
        case epoc::mmf_encoding_8bit_alaw:
        case epoc::mmf_encoding_8bit_mulaw:
            bb = 1;
            break;

        case epoc::mmf_encoding_16bit_pcm:
            bb = 2;
            break;

        default:
            return 0;
        }

        return bb * channels_;
    }

    mmf_dev_server::mmf_dev_server(eka2l1::system *sys)
        : service::typical_server(sys, MMF_DEV_SERVER_NAME) {
    }

    void mmf_dev_server::connect(service::ipc_context &context) {
        create_session<mmf_dev_server_session>(&context);
        context.complete(epoc::error_none);
    }

    mmf_dev_server_session::mmf_dev_server_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version)
        , last_buffer_(0)
        , buffer_chunk_(nullptr)
        , last_buffer_handle_(0)
        , stream_state_(epoc::mmf_state_idle)
        , desired_state_(epoc::mmf_state_idle)
        , stream_(nullptr)
        , finished_(false) {
        conf_ = get_caps();

        kernel_system *kern = serv->get_system()->get_kernel_system();
    }

    void mmf_dev_server_session::init_stream_through_state() {
        drivers::audio_driver *drv = server<mmf_dev_server>()->get_system()->get_audio_driver();

        switch (desired_state_) {
        case epoc::mmf_state_playing:
        case epoc::mmf_state_tone_playing:
            stream_ = drivers::new_dsp_out_stream(drv, drivers::dsp_stream_backend::dsp_stream_backend_ffmpeg);
            stream_->set_properties(8000, 2);

            break;

        default:
            LOG_ERROR(SERVICE_MMFAUD, "Recording stream is not supported!");
            return;
        }

        // Register complete callback
        stream_->register_callback(drivers::dsp_stream_notification_buffer_copied, [this](void *userdata) {
            kernel_system *kern = server<mmf_dev_server>()->get_kernel_object_owner();
            kern->lock();
            
            // Lock the access to this variable
            const std::lock_guard<std::mutex> guard(dev_access_lock_);
            
            if (finish_info_.empty()) {
                finished_ = false;
            } else {
                finish_info_.complete(epoc::error_none);
                finished_ = true;
            }

            stream_state_ = epoc::mmf_state_ready;
            do_get_buffer_to_be_filled();

            kern->unlock();
        }, nullptr);
    }

    void mmf_dev_server_session::max_volume(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        settings->max_vol_ = 100;

        ctx->write_data_to_descriptor_argument<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::volume(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        settings->vol_ = static_cast<std::int32_t>(reinterpret_cast<drivers::dsp_output_stream*>(
            stream_.get())->volume());

        ctx->write_data_to_descriptor_argument<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::set_priority_settings(service::ipc_context *ctx) {
        std::optional<epoc::mmf_priority_settings> pri_settings = ctx->get_argument_data_from_descriptor<epoc::mmf_priority_settings>(1);

        if (!pri_settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        pri_ = std::move(pri_settings.value());
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::set_volume(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(1);

        if (!settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::uint32_t final_volume = common::clamp<std::uint32_t>(0, 100, settings->vol_);
        (reinterpret_cast<drivers::dsp_output_stream*>(stream_.get()))->volume(final_volume);

        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::init3(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::init0(service::ipc_context *ctx) {
        if (stream_ && stream_->is_playing()) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        std::optional<epoc::mmf_dev_sound_proxy_settings> state_settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(1);

        if (!state_settings.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::mmf_state state_wanted = static_cast<epoc::mmf_state>(state_settings->state_);
        if ((state_wanted != epoc::mmf_state_playing) && (state_wanted != epoc::mmf_state_tone_playing)
            && (state_wanted != epoc::mmf_state_playing_recording)) {
            ctx->complete(epoc::error_not_supported);
        }

        desired_state_ = state_wanted;
        init_stream_through_state();

        ctx->complete(epoc::error_none);
    }

    epoc::mmf_capabilities mmf_dev_server_session::get_caps() {
        epoc::mmf_capabilities caps;

        // Fill our preferred settings
        caps.channels_ = 2;
        caps.encoding_ = epoc::mmf_encoding_16bit_pcm;
        caps.rate_ = epoc::mmf_sample_rate_44100hz;
        caps.buffer_size_ = duration_to_bytes(epoc::TO_BE_FILLED_DURATION, caps);

        return caps;
    }

    void mmf_dev_server_session::capabilities(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        settings->caps_ = get_caps();

        ctx->write_data_to_descriptor_argument<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::set_config(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(1);

        if (!settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        conf_ = settings->conf_;

        const std::uint32_t freq = freq_enum_to_number(conf_.rate_);
        std::uint32_t target_fourcc = 0;
        
        switch (conf_.encoding_) {
        case epoc::mmf_encoding_8bit_pcm:
            target_fourcc = drivers::PCM8_FOUR_CC_CODE;
            break;

        case epoc::mmf_encoding_16bit_pcm:
            target_fourcc = drivers::PCM16_FOUR_CC_CODE;
            break;

        default:
            LOG_ERROR(SERVICE_MMFAUD, "Unsupported audio encoding {}", static_cast<int>(conf_.encoding_));
            ctx->complete(epoc::error_not_supported);
            return;
        }

        stream_->set_properties(freq, conf_.channels_);
        if (!stream_->format(target_fourcc)) {
            LOG_ERROR(SERVICE_MMFAUD, "Failed to set target audio format");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::get_config(service::ipc_context *ctx) {
        std::optional<epoc::mmf_dev_sound_proxy_settings> settings
            = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_sound_proxy_settings>(2);

        if (!settings) {
            ctx->complete(epoc::error_argument);
            return;
        }

        settings->conf_ = conf_;

        ctx->write_data_to_descriptor_argument<epoc::mmf_dev_sound_proxy_settings>(2, settings.value());
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::get_supported_input_data_types() {
        four_ccs_.clear();
        four_ccs_.push_back(PCM8_CC);
        four_ccs_.push_back(PCM16_CC);
    }

    void mmf_dev_server_session::get_supported_input_data_types(service::ipc_context *ctx) {
        get_supported_input_data_types();
        ctx->write_data_to_descriptor_argument<int>(2, static_cast<int>(four_ccs_.size()));
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::copy_fourcc_array(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument(2, reinterpret_cast<const std::uint8_t *>(four_ccs_.data()),
            static_cast<std::uint32_t>(four_ccs_.size() * sizeof(std::uint32_t)));

        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::samples_played(service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument<std::uint32_t>(2, stream_->samples_played());
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::stop(service::ipc_context *ctx) {
        stream_state_ = epoc::mmf_state_dead;
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::play_init(service::ipc_context *ctx) {
        stream_state_ = epoc::mmf_state_ready;

        // Launch the first buffer ready to be filled
        do_get_buffer_to_be_filled();
        stream_->start();

        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::async_command(service::ipc_context *ctx) {
        auto pack = ctx->get_argument_data_from_descriptor<epoc::mmf_msg_destination>(0);
        std::optional<std::string> arg1 = ctx->get_argument_value<std::string>(1);
        std::optional<std::string> arg2 = ctx->get_argument_value<std::string>(2);

        LOG_TRACE(SERVICE_MMFAUD, "Async command stubbed");
    }

    void mmf_dev_server_session::request_resource_notification(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_MMFAUD, "Request resource notification stubbed");
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::cancel_play_error(service::ipc_context *ctx) {
        const std::lock_guard<std::mutex> guard(dev_access_lock_);

        finish_info_.complete(epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::do_get_buffer_to_be_filled() {
        if (buffer_fill_info_.empty()) {
            return;
        }
        
        kernel_system *kern = server<mmf_dev_server>()->get_kernel_object_owner();
        epocver ver_use = kern->get_epoc_version();

        kernel::handle return_value = 0;

        if (!buffer_chunk_ || (buffer_chunk_->max_size() < conf_.buffer_size_)) {
            // Recreate a new chunk that satisify the configuration
            // overwrite the buffer, the client side will close and destroy it laterz
            buffer_chunk_ = kern->create<kernel::chunk>(kern->get_memory_system(), nullptr,
                fmt::format("MMFBufferDevChunk{}", client_ss_uid_), 0, conf_.buffer_size_,
                conf_.buffer_size_, prot_read_write, kernel::chunk_type::normal,
                kernel::chunk_access::kernel_mapping, kernel::chunk_attrib::none);

            last_buffer_handle_ = kern->open_handle_with_thread(buffer_fill_info_.requester, buffer_chunk_, kernel::owner_type::thread);
            return_value = last_buffer_handle_;
            
            if (ver_use <= epocver::epoc94) {
                (reinterpret_cast<epoc::mmf_dev_hw_buf_v1*>(buffer_fill_buf_))->chunk_op_ = epoc::mmf_dev_chunk_op_open;
            } else {
                buffer_fill_buf_->chunk_op_ = epoc::mmf_dev_chunk_op_open;
            }
        } else {
            if (ver_use <= epocver::epoc94) {
                (reinterpret_cast<epoc::mmf_dev_hw_buf_v1*>(buffer_fill_buf_))->chunk_op_ = epoc::mmf_dev_chunk_op_none;
            } else {
                buffer_fill_buf_->chunk_op_ = epoc::mmf_dev_chunk_op_none;
            }
        }

        if (last_buffer_handle_ == kernel::INVALID_HANDLE) {
            buffer_fill_info_.complete(epoc::error_general);
            return;
        }

        const std::uint32_t max_request_size_align = common::align(conf_.buffer_size_, conf_.average_bytes_per_sample(), 0);

        if (ver_use <= epocver::epoc94) {
            auto buf_old = (reinterpret_cast<epoc::mmf_dev_hw_buf_v1*>(buffer_fill_buf_));

            buf_old->buffer_size_ = static_cast<std::uint32_t>(buffer_chunk_->max_size());
            buf_old->request_size_ = common::min<std::uint32_t>(max_request_size_align, duration_to_bytes(
                epoc::TO_BE_FILLED_DURATION, conf_));
        } else {
            buffer_fill_buf_->buffer_size_ = static_cast<std::uint32_t>(buffer_chunk_->max_size());
            buffer_fill_buf_->request_size_ = common::min<std::uint32_t>(max_request_size_align, duration_to_bytes(
                epoc::TO_BE_FILLED_DURATION, conf_));
        }

        buffer_fill_info_.complete(return_value);
    }

    void mmf_dev_server_session::get_buffer_to_be_filled(service::ipc_context *ctx) {
        if (!buffer_fill_info_.empty()) {
            // A pending PlayError is here.
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        const std::lock_guard<std::mutex> guard(dev_access_lock_);

        buffer_fill_info_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
        buffer_fill_buf_ = reinterpret_cast<epoc::mmf_dev_hw_buf_v2*>(ctx->get_descriptor_argument_ptr(2));

        if (!buffer_fill_buf_) {
            buffer_fill_info_.complete(epoc::error_argument);
            return;
        }

        if (stream_state_ == epoc::mmf_state_ready) {
            // Just get it already
            do_get_buffer_to_be_filled();
        }
    }

    void mmf_dev_server_session::play_error(service::ipc_context *ctx) {
        const std::lock_guard<std::mutex> guard(dev_access_lock_);

        if (finished_) {
            finish_info_.complete(epoc::error_none);
            finished_ = false;
            return;
        }

        finish_info_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void mmf_dev_server_session::play_data(service::ipc_context *ctx) {
        if (stream_state_ != epoc::mmf_state_ready) {
            ctx->complete(epoc::error_general);
            return;
        }

        const std::lock_guard<std::mutex> guard(dev_access_lock_);
        finished_ = false;

        kernel_system *kern = server<mmf_dev_server>()->get_kernel_object_owner();
        epocver ver_use = kern->get_epoc_version();

        std::uint32_t supplied_size = 0;
        if (ver_use <= epocver::epoc94) {
            std::optional<epoc::mmf_dev_hw_buf_v1> buf_old = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_hw_buf_v1>(1);
            if (!buf_old.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            supplied_size = buf_old->buffer_size_;
            last_buffer_ = buf_old->last_buffer_;
        } else {
            std::optional<epoc::mmf_dev_hw_buf_v2> buf_new = ctx->get_argument_data_from_descriptor<epoc::mmf_dev_hw_buf_v2>(1);
            if (!buf_new.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            supplied_size = buf_new->buffer_size_;
            last_buffer_ = buf_new->last_buffer_;
        }

        stream_state_ = epoc::mmf_state_playing;

        drivers::dsp_output_stream *out_stream = reinterpret_cast<drivers::dsp_output_stream*>(stream_.get());
        out_stream->write(reinterpret_cast<std::uint8_t*>(buffer_chunk_->host_base()), supplied_size);

        ctx->complete(epoc::error_none);
    }

    void mmf_dev_server_session::fetch(service::ipc_context *ctx) {
        const epocver ver = server<mmf_dev_server>()->get_kernel_object_owner()->get_epoc_version();

        if (ver == epocver::epoc93) {
            switch (ctx->msg->function) {
            case epoc::mmf_dev_init0:
                init0(ctx);
                break;

            case epoc::mmf_dev_config:
                get_config(ctx);
                break;

            case epoc::mmf_dev_set_config:
                set_config(ctx);
                break;

            case epoc::mmf_dev_capabilities:
                capabilities(ctx);
                break;

            case epoc::mmf_dev_volume:
                volume(ctx);
                break;

            case epoc::mmf_dev_max_volume:
                max_volume(ctx);
                break;

            case epoc::mmf_dev_set_volume:
                set_volume(ctx);
                break;

            case epoc::mmf_dev_buffer_to_be_filled:
                get_buffer_to_be_filled(ctx);
                break;

            case epoc::mmf_dev_play_complete_notify:
                play_error(ctx);
                break;

            case epoc::mmf_dev_play_init:
                play_init(ctx);
                break;

            case epoc::mmf_dev_play_data:
                play_data(ctx);
                break;

            case epoc::mmf_dev_set_priority_settings:
                set_priority_settings(ctx);
                break;

            /*
            case epoc::mmf_dev_cancel_play_complete_notify:
                cancel_play_error(ctx);
                break;

            case epoc::mmf_dev_stop:
                stop(ctx);
                break;

            case epoc::mmf_dev_get_supported_input_data_types:
                get_supported_input_data_types(ctx);
                break;

            case epoc::mmf_dev_copy_fourcc_array:
                copy_fourcc_array(ctx);
                break;

            case epoc::mmf_dev_samples_played:
                samples_played(ctx);
                break;
            */

            default:
                LOG_ERROR(SERVICE_MMFAUD, "Unimplemented MMF dev server session opcode {}", ctx->msg->function);
                break;
            }
        } else {
            LOG_ERROR(SERVICE_MMFAUD, "Unimplemented MMF dev server session opcode {}", ctx->msg->function);
        }
    }
}