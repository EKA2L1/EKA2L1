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

#pragma once

#include <dispatch/def.h>
#include <mem/ptr.h>
#include <utils/reqsts.h>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_player_inst, void *);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_destroy, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_supply_url, eka2l1::ptr<void> handle,
        const std::uint16_t *url, const std::uint32_t url_length);

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_play, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_stop, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_notify_any_done, eka2l1::ptr<void> handle,
        eka2l1::ptr<epoc::request_status> sts);

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_cancel_notify_done, eka2l1::ptr<void> handle);

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_max_volume, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_volume, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_volume, eka2l1::ptr<void> handle, const std::int32_t volume);

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_repeats, eka2l1::ptr<void> handle, const std::int32_t times, const std::uint64_t silence_interval_micros);

    // DSP streams
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_dsp_out_stream_create, void *);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_destroy, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_set_properties, eka2l1::ptr<void> handle, const std::uint32_t freq, const std::uint32_t channels);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_start, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_write, eka2l1::ptr<void> handle, const std::uint8_t *data, const std::uint32_t data_size);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> req);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_max_volume, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_volume, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_set_volume, eka2l1::ptr<void> handle, std::uint32_t vol);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_stop, eka2l1::ptr<void> handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_bytes_rendered, eka2l1::ptr<void> handle, std::uint64_t *bytes);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready_cancel, eka2l1::ptr<void> handle);
}