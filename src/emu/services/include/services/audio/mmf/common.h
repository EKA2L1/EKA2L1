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

#include <cstdint>

namespace eka2l1::epoc {
    enum mmf_state : std::uint32_t {
        mmf_state_idle = 0,
        mmf_state_playing = 1,
        mmf_state_tone_playing = 2,
        mmf_state_recording = 3,
        mmf_state_playing_recording = 4,
        mmf_state_converting = 5
    };

    enum mmf_encoding : std::uint32_t {
        mmf_encoding_8bit_pcm = 1 << 0,
        mmf_encoding_16bit_pcm = 1 << 1,
        mmf_encoding_8bit_alaw = 1 << 2,
        mmf_encoding_8bit_mulaw = 1 << 3
    };

    enum mmf_sample_rate : std::uint32_t {
        mmf_sample_rate_8000hz = 1 << 0,
        mmf_sample_rate_11025hz = 1 << 1,
        mmf_sample_rate_16000hz = 1 << 2,
        mmf_sample_rate_22050hz = 1 << 3,
        mmf_sample_rate_32000hz = 1 << 4,
        mmf_sample_rate_44100hz = 1 << 5,
        mmf_sample_rate_48000hz = 1 << 6,
        mmf_sample_rate_88200hz = 1 << 7,
        mmf_sample_rate_96000hz = 1 << 8,
        mmf_sample_rate_12000hz = 1 << 9,
        mmf_sample_rate_24000hz = 1 << 10,
        mmf_sample_rate_64000hz = 1 << 11
    };

    inline mmf_sample_rate operator | (mmf_sample_rate a, mmf_sample_rate b) {
        return static_cast<mmf_sample_rate>(static_cast<int>(a) | static_cast<int>(b));
    }

    static constexpr std::uint32_t TARGET_REQUEST_SAMPLES = 512;

    struct mmf_capabilities {
        std::uint32_t rate_;
        mmf_encoding encoding_;
        std::uint32_t channels_;
        std::int32_t buffer_size_;

        std::uint32_t average_bytes_per_sample() const;
        std::uint32_t buffer_size_recommended() const;
    };

    struct mmf_dev_sound_proxy_settings {
        mmf_state state_;
        std::uint32_t hw_dev_uid_;
        std::uint32_t four_cc_;
        mmf_capabilities caps_;
        mmf_capabilities conf_;
        std::uint32_t max_vol_;
        std::int32_t vol_;
        std::int32_t max_gain_;
        std::int32_t gain_;
        std::int32_t left_percentage_;
        std::int32_t right_percentage_;
        std::int32_t freq_one_;
        std::int32_t freq_two_;
        std::uint64_t duration_;
        std::uint32_t tone_on_length_;
        std::uint32_t tone_off_length_;
        std::uint32_t pause_length_;
        //std::uint32_t nof_event_uid_;
        //std::uint32_t interface_;
    };

    struct mmf_priority_settings {
        std::int32_t priority_;
        std::int32_t pref_;
        mmf_state state_;
    };

    enum mmf_dev_server_opcode {
        mmf_dev_init0 = 0,
        mmf_dev_init3 = 3,
        mmf_dev_capabilities = 4,
        mmf_dev_config = 5,
        mmf_dev_set_config = 6,
        mmf_dev_max_volume = 7,
        mmf_dev_volume = 8,
        mmf_dev_set_volume = 9,
        mmf_dev_max_gain = 10,
        mmf_dev_gain = 11,
        mmf_dev_set_gain = 12,
        mmf_dev_play_balance = 13,
        mmf_dev_set_play_balance = 14,
        // Two of this are guessed from RE and from error I get stubbing
        mmf_dev_get_buffer = 19,
        mmf_dev_cancel_get_buffer = 20,
        mmf_dev_complete_notify = 21,
        mmf_dev_cancel_complete_notify = 22,
        mmf_dev_play_init = 25,
        mmf_dev_record_init = 26,
        mmf_dev_play_data = 27,
        mmf_dev_record_data = 28,
        mmf_dev_stop = 29,
        /*
        mmf_dev_play_dtmf_string_length = 34,
        */
        mmf_dev_set_volume_ramp = 37,
        mmf_dev_get_supported_input_data_types = 38,
        mmf_dev_copy_fourcc_array = 40,
        mmf_dev_samples_played = 42,
        mmf_dev_set_priority_settings = 44
    };

    // Should/near the same as the one on Symbian OSS
    // The S^3 version does not call server to register callback (request like BufferToBeFilled or Emptied),
    // but rather run a message queue and respond accordingly to reported back event
    // For example if a event named new BTBF is received from message queue, they will call GetBufferToBeFilled
    enum mmf_dev_server_newarch_opcode {
        mmf_dev_newarch_post_open = 0,
        mmf_dev_newarch_init0 = 1,
        mmf_dev_newarch_init1 = 2,
        mmf_dev_newarch_init2 = 3,
        mmf_dev_newarch_init3 = 4,
        mmf_dev_newarch_capabilities = 5,
        mmf_dev_newarch_config = 6,
        mmf_dev_newarch_set_config = 7,
        mmf_dev_newarch_max_volume = 8,
        mmf_dev_newarch_volume = 9,
        mmf_dev_newarch_set_volume = 10,
        mmf_dev_newarch_max_gain = 11,
        mmf_dev_newarch_gain = 12,
        mmf_dev_newarch_set_gain = 13,
        mmf_dev_newarch_play_balance = 14,
        mmf_dev_newarch_set_play_balance = 15,
        mmf_dev_newarch_record_balance = 16,
        mmf_dev_newarch_set_record_balance = 17,
        // The list here are event codes
        mmf_dev_newarch_init_complete_evt = 18,
        mmf_dev_newarch_btbf_evt = 19,
        mmf_dev_newarch_btbe_evt = 20,
        mmf_dev_newarch_play_error_evt = 21,
        mmf_dev_newarch_record_error_evt = 22,
        mmf_dev_newarch_tone_done_evt = 23,
        mmf_dev_newarch_send_evt_to_cli_complete_evt = 24,
        // End event code
        mmf_dev_newarch_play_init = 25,
        mmf_dev_newarch_record_init = 26,
        mmf_dev_newarch_play_data = 27,
        mmf_dev_newarch_record_data = 28,
        mmf_dev_newarch_stop = 29,
        mmf_dev_newarch_pause = 30,
        mmf_dev_newarch_play_tone = 31,
        mmf_dev_newarch_play_dual_tone = 32,
        mmf_dev_newarch_play_dtmf_string = 33,
        mmf_dev_newarch_play_dtmf_string_len = 34,
        mmf_dev_newarch_play_tone_seq = 35,
        mmf_dev_newarch_play_fixed_seq = 36,
        mmf_dev_newarch_set_dtmf_len = 37,
        mmf_dev_newarch_set_volume_ramp = 38,
        mmf_dev_newarch_get_supported_input_data_types = 39,
        mmf_dev_newarch_get_supported_output_data_types = 40,
        mmf_dev_newarch_copy_fourcc_array_data = 41,
        mmf_dev_newarch_samples_recorded = 42,
        mmf_dev_newarch_samples_played = 43,
        mmf_dev_newarch_set_tone_repeats = 44,
        mmf_dev_newarch_set_priority_settings = 45,
        mmf_dev_newarch_fixed_sequence_name = 46,
        mmf_dev_newarch_custom_interface = 47,
        mmf_dev_newarch_fixed_sequence_count = 48,
        mmf_dev_newarch_request_resource_nof = 49,
        mmf_dev_newarch_cancel_request_resource_nof = 50,
        mmf_dev_newarch_get_resources_nof_data = 51,
        mmf_dev_newarch_will_resume_play = 52,
        mmf_dev_newarch_empty_buffers = 53,
        mmf_dev_newarch_cancel_init = 54,
        mmf_dev_newarch_btbf_data = 55,
        mmf_dev_newarch_btbe_data = 56,
        mmf_dev_newarch_close = 62,
        mmf_dev_newarch_paused_record_complete_evt = 63,
        mmf_dev_newarch_is_resume_supported = 66,
        mmf_dev_newarch_resume = 67
    };

    struct mmf_msg_destination {
        std::uint32_t interface_id_;
        std::int32_t dest_handle_;
        std::uint32_t reserved[3];
    };

    enum mmf_dev_chunk_op : std::uint32_t {
        mmf_dev_chunk_op_none = 0,
        mmf_dev_chunk_op_open = 1
    };

    struct mmf_dev_hw_buf_v1 {
        std::int32_t request_size_; ///< The size of audio server needs.
        std::int32_t last_buffer_; ///< Mark to the server this is the last buffer gonna be sent.
        std::int32_t buffer_size_; ///< Total size the audio chunk offered.

        mmf_dev_chunk_op chunk_op_; ///< Request that the client side should reopen the chunk handle.
            ///< May occur due to chunk recreating
    };

    static_assert(sizeof(mmf_dev_hw_buf_v1) == 16);

    struct mmf_dev_hw_buf_v2 {
        std::uint32_t buffer_type_; ///< UID of the buffer.
        std::int32_t request_size_; ///< The size of audio server needs.
        std::int32_t last_buffer_; ///< Mark to the server this is the last buffer gonna be sent.
        std::int32_t buffer_size_; ///< Total size the audio chunk offered.

        mmf_dev_chunk_op chunk_op_; ///< Request that the client side should reopen the chunk handle.
            ///< May occur due to chunk recreating
    };

    struct mmf_event {
        std::uint32_t type_;
        std::uint32_t error_code_;
        std::uint32_t reserved1_;
        std::uint32_t aligned_padding_;
    };

#pragma pack(push, 1)
    struct mmf_dev_sound_queue_item {
        std::uint32_t event_code_;
        std::uint32_t padding_;
        std::uint32_t event_struct_size_ = sizeof(mmf_event);
        std::uint32_t event_max_struct_size_ = sizeof(mmf_event);
        mmf_event event_;
        std::uint32_t error_code_;
        std::uint32_t padding2_;

        explicit mmf_dev_sound_queue_item(const std::uint32_t code, const std::uint32_t error = 0)
            : event_code_(code)
            , error_code_(error) {
        }
    };
#pragma pack(pop)
}