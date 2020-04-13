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
                reinterpret_cast<epoc::notify_info *>(data)->complete(epoc::error_none);
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
}