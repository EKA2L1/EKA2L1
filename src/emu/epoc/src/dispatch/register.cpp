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
#include <epoc/dispatch/register.h>
#include <epoc/dispatch/screen.h>

namespace eka2l1::dispatch {
    const eka2l1::hle::func_map dispatch_funcs = {
        BRIDGE_REGISTER(1, update_screen),
        BRIDGE_REGISTER(2, fast_blit),
        BRIDGE_REGISTER(0x20, eaudio_player_inst),
        BRIDGE_REGISTER(0x21, eaudio_player_notify_any_done),
        BRIDGE_REGISTER(0x22, eaudio_player_supply_url),
        BRIDGE_REGISTER(0x24, eaudio_player_set_volume),
        BRIDGE_REGISTER(0x25, eaudio_player_get_volume),
        BRIDGE_REGISTER(0x26, eaudio_player_max_volume),
        BRIDGE_REGISTER(0x27, eaudio_player_play),
        BRIDGE_REGISTER(0x28, eaudio_player_stop),
        BRIDGE_REGISTER(0x2A, eaudio_player_cancel_notify_done),
        BRIDGE_REGISTER(0x30, eaudio_player_set_repeats),
        BRIDGE_REGISTER(0x31, eaudio_player_destroy),
        BRIDGE_REGISTER(0x40, eaudio_dsp_out_stream_create),
        BRIDGE_REGISTER(0x41, eaudio_dsp_out_stream_write),
        BRIDGE_REGISTER(0x42, eaudio_dsp_stream_set_properties),
        BRIDGE_REGISTER(0x43, eaudio_dsp_stream_start),
        BRIDGE_REGISTER(0x44, eaudio_dsp_stream_stop),
        BRIDGE_REGISTER(0x47, eaudio_dsp_out_stream_set_volume),
        BRIDGE_REGISTER(0x48, eaudio_dsp_out_stream_max_volume),
        BRIDGE_REGISTER(0x49, eaudio_dsp_stream_notify_buffer_ready),
        BRIDGE_REGISTER(0x4B, eaudio_dsp_stream_destroy),
        BRIDGE_REGISTER(0x4C, eaudio_dsp_out_stream_volume),
        BRIDGE_REGISTER(0x4F, eaudio_dsp_stream_bytes_rendered)
    };
}