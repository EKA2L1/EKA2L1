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

namespace eka2l1::dispatch {
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
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        auto stream_new = drivers::new_dsp_out_stream(aud_driver, drivers::dsp_stream_backend_ffmpeg);

        if (!stream_new) {
            LOG_ERROR("Unable to create new DSP out stream!");
            return 0;
        }

        return dispatcher->dsp_streams_.add_object(stream_new);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        if (!dispatcher->dsp_streams_.remove_object(handle.ptr_address())) {
            return epoc::error_not_found;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_set_properties, eka2l1::ptr<void> handle, const std::uint32_t freq, const std::uint32_t channels) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->set_properties(freq, channels)) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_start, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->start()) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_stop, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->stop()) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_write, eka2l1::ptr<void> handle, const std::uint8_t *data, const std::uint32_t data_size) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream);
        
        if (!out_stream.write(data, data_size)) {
            return epoc::error_not_ready;
        }

        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> req) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream);

        epoc::notify_info *info = new epoc::notify_info;

        info->requester = sys->get_kernel_system()->crr_thread();
        info->sts = req;

        timing_system *timing = sys->get_timing_system();

        out_stream.register_callback(drivers::dsp_stream_notification_buffer_copied, [dispatcher, timing](void *userdata) {
            timing->schedule_event(40000, dispatcher->nof_complete_evt_, reinterpret_cast<std::uint64_t>(userdata), true); 
        }, info);

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready_cancel, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream);
        timing_system *timing = sys->get_timing_system();

        void *userdata = out_stream.get_userdata(drivers::dsp_stream_notification_buffer_copied);

        if (userdata) {
            const bool unschedule_ok = timing->unschedule_event(dispatcher->nof_complete_evt_, (std::uint64_t)userdata);

            if (unschedule_ok) {
                reinterpret_cast<epoc::notify_info*>(userdata)->complete(epoc::error_cancel);
                delete userdata;
            }

            // Deregister the callback
            out_stream.register_callback(drivers::dsp_stream_notification_buffer_copied, nullptr, nullptr);
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_max_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream);
        return static_cast<std::int32_t>(out_stream.max_volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream);
        return static_cast<std::int32_t>(out_stream.volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_set_volume, eka2l1::ptr<void> handle, std::uint32_t vol) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream&>(*stream);
        
        if (vol > out_stream.max_volume()) {
            return epoc::error_argument;
        }

        out_stream.volume(vol);
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_bytes_rendered, eka2l1::ptr<void> handle, std::uint64_t *bytes) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::dsp_stream *stream = dispatcher->dsp_streams_.get_object(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        *bytes = stream->bytes_rendered();
        return epoc::error_none;
    }
}