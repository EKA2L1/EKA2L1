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

#include <epoc/dispatch/audio.h>
#include <epoc/dispatch/dispatcher.h>
#include <epoc/vfs.h>

#include <epoc/kernel.h>
#include <epoc/utils/err.h>
#include <epoc/utils/reqsts.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <common/time.h>

namespace eka2l1::dispatch {
    dsp_epoc_stream::dsp_epoc_stream(std::unique_ptr<drivers::dsp_stream> &stream)
        : ll_stream_(std::move(stream)) {
    }

    dsp_epoc_stream::~dsp_epoc_stream() {
        audio_event *evt = evt_queue_.next_;

        while (evt) {
            audio_event *to_del = evt;
            evt = evt->next_;

            delete to_del;
        }
    }

    audio_event *dsp_epoc_stream::get_event(const eka2l1::ptr<epoc::request_status> req_sts) {
        audio_event *evt = &evt_queue_;

        while (evt->next_ != nullptr) {
            if (evt->info_.sts == req_sts) {
                return evt;
            }

            evt = evt->next_;
        }

        evt->next_ = new audio_event;
        return evt->next_;
    }

    void dsp_epoc_stream::delete_event(audio_event *the_evt) {
        audio_event *evt = &evt_queue_;

        while (evt->next_ != nullptr) {
            if (evt->next_ == the_evt) {
                evt->next_ = the_evt->next_;
                delete the_evt;

                return;
            }

            evt = evt->next_;
        }
    }

    void dsp_epoc_stream::deliver_audio_events(kernel_system *kern, timing_system *timing) {
        const std::lock_guard<std::mutex> guard(lock_);
        audio_event *evt = evt_queue_.next_;

        while (evt != nullptr) {
            if (evt->flags_ & audio_event::FLAG_COMPLETED) {
                const std::uint64_t delta = timing->ticks() - evt->start_ticks_;
                const std::uint64_t ticks_target = timing->us_to_cycles(common::get_current_time_in_microseconds_since_epoch()
                    - evt->start_host_);

                if (delta < ticks_target)
                    timing->add_ticks(static_cast<std::uint32_t>(ticks_target - delta));

                kern->lock();
                evt->info_.complete(epoc::error_none);
                kern->unlock();

                delete_event(evt);
            }

            evt = evt->next_;
        }
    }

    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_player_inst, void *) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        auto player_new = drivers::new_audio_player(aud_driver, drivers::get_suitable_player_type());

        if (!player_new) {
            LOG_ERROR("Unable to instantiate new audio player!");
            return 0;
        }

        return dispatcher->audio_players_.add_object(player_new);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        if (!dispatcher->audio_players_.remove_object(handle.ptr_address())) {
            return epoc::error_not_found;
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
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->queue_url(common::ucs2_to_utf8(url_str))) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_play, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->play()) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_stop, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->stop()) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_max_volume, eka2l1::ptr<void> handle) {
        return 100;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return eplayer->volume();
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_volume, eka2l1::ptr<void> handle, const std::int32_t volume) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->set_volume(volume)) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_notify_any_done, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> sts) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        epoc::notify_info info;

        info.requester = sys->get_kernel_system()->crr_thread();
        info.sts = sts;

        if (!eplayer->notify_any_done([eplayer](std::uint8_t *data) {
                epoc::notify_info *info = reinterpret_cast<epoc::notify_info *>(data);
                kernel_system *kern = info->requester->get_kernel_object_owner();

                kern->lock();
                info->complete(epoc::error_none);
                kern->unlock();

                eplayer->clear_notify_done();
            },
                reinterpret_cast<std::uint8_t *>(&info), sizeof(epoc::notify_info))) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_cancel_notify_done, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        std::uint8_t *notify = eplayer->get_notify_userdata(nullptr);

        if (!notify) {
            return epoc::error_none;
        }

        reinterpret_cast<epoc::notify_info *>(notify)->complete(epoc::error_cancel);
        eplayer->clear_notify_done();

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_repeats, eka2l1::ptr<void> handle, const std::int32_t times, const std::uint64_t silence_interval_micros) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::player *eplayer = dispatcher->audio_players_.get_object(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->set_repeat(times, silence_interval_micros);
        return epoc::error_none;
    }

    // DSP streams
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_dsp_out_stream_create, void*) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        timing_system *timing = sys->get_timing_system();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        auto ll_stream = drivers::new_dsp_out_stream(aud_driver, drivers::dsp_stream_backend_ffmpeg);

        if (!ll_stream) {
            LOG_ERROR("Unable to create new DSP out stream!");
            return 0;
        }

        drivers::dsp_stream *ll_stream_ptr = ll_stream.get();
        auto stream_new = std::make_unique<dsp_epoc_stream>(ll_stream);

        timing->schedule_event(40000, dispatcher->audio_nof_complete_evt_, reinterpret_cast<std::uint64_t>(stream_new.get()));

        return dispatcher->dsp_streams_.add_object(stream_new);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        timing_system *timing = sys->get_timing_system();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        timing->unschedule_event(dispatcher->audio_nof_complete_evt_, reinterpret_cast<std::uint64_t>(stream));
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
        timing_system *timing = sys->get_timing_system();

        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->start()) {
            return epoc::error_general;
        }

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

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_write, eka2l1::ptr<void> handle, const std::uint8_t *data, const std::uint32_t data_size) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream->ll_stream_);
        
        if (!out_stream.write(data, data_size)) {
            return epoc::error_not_ready;
        }

        return epoc::error_none;
    }
    
    audio_event::audio_event()
        : start_ticks_(0)
        , next_(nullptr)
        , flags_(0) {

    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> req) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream->ll_stream_);
        timing_system *timing = sys->get_timing_system();

        const std::lock_guard<std::mutex> guard(stream->lock_);
            
        audio_event *evt = stream->get_event(req);
        evt->info_.requester = sys->get_kernel_system()->crr_thread();
        evt->info_.sts = req;
        evt->start_ticks_ = timing->ticks();
        evt->start_host_ = common::get_current_time_in_microseconds_since_epoch();

        out_stream.register_callback(drivers::dsp_stream_notification_buffer_copied, [dispatcher](void *userdata) {
            dsp_epoc_stream *epoc_stream = reinterpret_cast<dsp_epoc_stream*>(userdata);
            
            const std::lock_guard<std::mutex> guard(epoc_stream->lock_);
            audio_event *evt = epoc_stream->evt_queue_.next_;

            while (evt && (evt->flags_ & audio_event::FLAG_COMPLETED)) {
                evt = evt->next_;
            }

            evt->flags_ |= audio_event::FLAG_COMPLETED;
        }, stream);
        
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready_cancel, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        timing_system *timing = sys->get_timing_system();
        
        {
            const std::lock_guard<std::mutex> guard(stream->lock_);
            audio_event *aud_evt = stream->evt_queue_.next_;

            stream->evt_queue_.next_ = nullptr;

            while (aud_evt) {
                aud_evt->info_.complete(epoc::error_cancel);

                audio_event *to_del = aud_evt;
                aud_evt = aud_evt->next_;

                delete to_del;
            }
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_max_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream->ll_stream_);
        return static_cast<std::int32_t>(out_stream.max_volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream->ll_stream_);
        return static_cast<std::int32_t>(out_stream.volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_set_volume, eka2l1::ptr<void> handle, std::uint32_t vol) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dsp_epoc_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream->ll_stream_);
        
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
}