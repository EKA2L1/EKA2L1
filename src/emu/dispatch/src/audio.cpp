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

#include <dispatch/audio.h>
#include <dispatch/dispatcher.h>
#include <vfs/vfs.h>

#include <system/epoc.h>

#include <kernel/kernel.h>
#include <utils/err.h>
#include <utils/reqsts.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <common/time.h>

#include <utils/des.h>

namespace eka2l1::dispatch {
    dsp_epoc_audren_sema::dsp_epoc_audren_sema()
        : own_(0) {
    }

    bool dsp_epoc_audren_sema::free() const {
        return !own_.load();
    }

    // Argument reserved
    void dsp_epoc_audren_sema::acquire(void * /* owner */) {
        own_++;
    }

    void dsp_epoc_audren_sema::release(void * /* owner */) {
        own_--;
    }
    
    dsp_epoc_stream::dsp_epoc_stream(std::unique_ptr<drivers::dsp_stream> &stream, dsp_epoc_audren_sema *sema)
        : ll_stream_(std::move(stream))
        , audren_sema_(sema) {
    }

    dsp_epoc_stream::~dsp_epoc_stream() {
        
    }

    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_player_inst, const std::uint32_t init_flags) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        auto player_new = drivers::new_audio_player(aud_driver, drivers::get_suitable_player_type());

        if (!player_new) {
            LOG_ERROR("Unable to instantiate new audio player!");
            return 0;
        }

        std::unique_ptr<dsp_epoc_player> player_epoc = std::make_unique<dsp_epoc_player>(player_new,
            dispatcher->get_audren_sema(), init_flags);

        return dispatcher->audio_players_.add_object(player_epoc);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (eplayer->impl_->is_playing()) {
            eplayer->impl_->stop();
            eplayer->audren_sema_->release(eplayer);
        }

        if (!dispatcher->audio_players_.remove_object(handle.ptr_address())) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_supply_url, eka2l1::ptr<void> handle,
        const std::uint16_t *url, const std::uint32_t url_length) {
        std::u16string url_str(reinterpret_cast<const char16_t *>(url), url_length);

        // Check if the URL references to a local path (drive)
        const std::u16string root = eka2l1::root_path(url_str, true);

        if ((root.length() >= 2) && (root[1] == u':')) {
            // It should be a local path. Transform it to host path
            std::optional<std::u16string> host_path = sys->get_io_system()->get_raw_path(url_str);

            if (host_path.has_value()) {
                url_str = std::move(host_path.value());
            }
        }

        // Proceed with the current URL
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->queue_url(common::ucs2_to_utf8(url_str))) {
            return epoc::error_not_supported;
        }

        if (eplayer->should_prepare_play_when_queue()) {
            if (!eplayer->impl_->prepare_play_newest()) {
                return epoc::error_general;
            }
        }

        // TODO: Return duration
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_supply_buffer, eka2l1::ptr<void> handle, epoc::des8 *buffer) {
        kernel::process *pr = sys->get_kernel_system()->crr_process();

        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->custom_stream_ = std::make_unique<epoc::rw_des_stream>(buffer, pr);
        if (!eplayer->impl_->queue_custom(reinterpret_cast<common::rw_stream*>(eplayer->custom_stream_.get()))) {
            return epoc::error_not_supported;
        }

        if (eplayer->should_prepare_play_when_queue()) {
            if (!eplayer->impl_->prepare_play_newest()) {
                return epoc::error_general;
            }
        }

        // TODO: Return duration
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_play, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->play()) {
            return epoc::error_general;
        }

        eplayer->audren_sema_->acquire(eplayer);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_stop, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->stop()) {
            return epoc::error_general;
        }

        eplayer->audren_sema_->free();
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_max_volume, eka2l1::ptr<void> handle) {
        return 100;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return eplayer->impl_->volume();
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_volume, eka2l1::ptr<void> handle, const std::int32_t volume) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_volume(volume)) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_notify_any_done, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> sts) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        epoc::notify_info info;

        info.requester = sys->get_kernel_system()->crr_thread();
        info.sts = sts;

        if (!eplayer->impl_->notify_any_done([eplayer](std::uint8_t *data) {
                epoc::notify_info *info = reinterpret_cast<epoc::notify_info *>(data);
                kernel_system *kern = info->requester->get_kernel_object_owner();

                kern->lock();
                info->complete(epoc::error_none);
                kern->unlock();

                eplayer->impl_->clear_notify_done();
            },
                reinterpret_cast<std::uint8_t *>(&info), sizeof(epoc::notify_info))) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_cancel_notify_done, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(eplayer->impl_->lock_);
        std::uint8_t *notify = eplayer->impl_->get_notify_userdata(nullptr);

        if (!notify) {
            return epoc::error_none;
        }

        reinterpret_cast<epoc::notify_info *>(notify)->complete(epoc::error_cancel);
        eplayer->impl_->clear_notify_done();

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_repeats, eka2l1::ptr<void> handle, const std::int32_t times, const std::uint64_t silence_interval_micros) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->impl_->set_repeat(times, silence_interval_micros);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_dest_sample_rate, eka2l1::ptr<void> handle){
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(eplayer->impl_->get_dest_freq());
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_sample_rate, eka2l1::ptr<void> handle, const std::int32_t sample_rate) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_dest_freq(sample_rate)) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_dest_channel_count, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(eplayer->impl_->get_dest_channel_count());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_channel_count, eka2l1::ptr<void> handle, const std::int32_t channel_count) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_dest_channel_count(channel_count)) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_dest_encoding, eka2l1::ptr<void> handle, std::uint32_t *encoding) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!encoding) {
            return epoc::error_argument;
        }

        *encoding = eplayer->impl_->get_dest_encoding();
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_encoding, eka2l1::ptr<void> handle, const std::uint32_t encoding) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_dest_encoding(encoding)) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_container_format, eka2l1::ptr<void> handle, const std::uint32_t container_format) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->impl_->set_dest_container_format(container_format);
        return epoc::error_none;
    }

    // DSP streams
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_dsp_out_stream_create, void *) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        ntimer *timing = sys->get_ntimer();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        auto ll_stream = drivers::new_dsp_out_stream(aud_driver, drivers::dsp_stream_backend_ffmpeg);

        if (!ll_stream) {
            LOG_ERROR("Unable to create new DSP out stream!");
            return 0;
        }

        drivers::dsp_stream *ll_stream_ptr = ll_stream.get();
        auto stream_new = std::make_unique<dsp_epoc_stream>(ll_stream, dispatcher->get_audren_sema());

        stream_new->ll_stream_->register_callback(
            drivers::dsp_stream_notification_buffer_copied, [](void *userdata) {
                dsp_epoc_stream *epoc_stream = reinterpret_cast<dsp_epoc_stream *>(userdata);
                const std::lock_guard<std::mutex> guard(epoc_stream->lock_);

                epoc::notify_info &info = epoc_stream->copied_info_;
                kernel_system *kern = info.requester->get_kernel_object_owner();

                kern->lock();
                info.complete(epoc::error_none);
                kern->unlock();
            },
            stream_new.get());

        return dispatcher->dsp_streams_.add_object(stream_new);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        ntimer *timing = sys->get_ntimer();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (stream->ll_stream_->is_playing() || stream->ll_stream_->is_recording()) {
            stream->ll_stream_->stop();
            stream->audren_sema_->release(stream);
        }

        dispatcher->dsp_streams_.remove_object(handle.ptr_address());
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_set_properties, eka2l1::ptr<void> handle, const std::uint32_t freq, const std::uint32_t channels) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->set_properties(freq, channels)) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_start, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        ntimer *timing = sys->get_ntimer();

        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->start()) {
            return epoc::error_general;
        }

        stream->audren_sema_->acquire(stream);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_stop, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->stop()) {
            return epoc::error_general;
        }

        stream->audren_sema_->release(stream);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_write, eka2l1::ptr<void> handle, const std::uint8_t *data, const std::uint32_t data_size) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*stream->ll_stream_);

        if (!out_stream.write(data, data_size)) {
            return epoc::error_not_ready;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> req) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*stream->ll_stream_);
        ntimer *timing = sys->get_ntimer();

        const std::lock_guard<std::mutex> guard(stream->lock_);

        stream->copied_info_ = epoc::notify_info { req, sys->get_kernel_system()->crr_thread() };
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready_cancel, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        ntimer *timing = sys->get_ntimer();
        const std::lock_guard<std::mutex> guard(stream->lock_);

        stream->copied_info_.complete(epoc::error_cancel);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_max_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*stream->ll_stream_);
        return static_cast<std::int32_t>(out_stream.max_volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*stream->ll_stream_);
        return static_cast<std::int32_t>(out_stream.volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_set_volume, eka2l1::ptr<void> handle, std::uint32_t vol) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*stream->ll_stream_);

        if (vol > out_stream.max_volume()) {
            return epoc::error_argument;
        }

        out_stream.volume(vol);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_bytes_rendered, eka2l1::ptr<void> handle, std::uint64_t *bytes) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        *bytes = stream->ll_stream_->bytes_rendered();
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_position, eka2l1::ptr<void> handle, std::uint64_t *the_time) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        // Well weird enough this position is calculated from the total sample copied
        // This is related to THPS. Where it checks for the duration when the buffer has just been copied.
        // If the duration played drains the buffer, only at that time it starts to supply more audio data.
        *the_time = stream->ll_stream_->position();
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_reset_stat, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        stream->ll_stream_->reset_stat();
        return epoc::error_none;
    } 
}