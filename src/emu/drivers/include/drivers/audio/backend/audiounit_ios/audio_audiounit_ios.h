/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <drivers/audio/audio.h>

#include <mutex>
#include <vector>

namespace eka2l1::drivers {
    struct audiounit_ios_stream_base;

    // iOS-native audio driver. Talks directly to AURemoteIO via AudioToolbox
    // and configures AVAudioSession (Playback + mix-with-others). Replaces
    // the cubeb wrapper on iOS — cubeb's macOS-targeted code path is full of
    // HAL types the iOS SDK doesn't ship, so a direct implementation is both
    // smaller and easier to keep working as iOS evolves.
    struct audiounit_ios_audio_driver : public audio_driver {
    private:
        std::mutex streams_mutex_;
        std::vector<audiounit_ios_stream_base *> streams_;
        std::mutex session_mutex_;
        std::size_t active_input_sessions_ = 0;

    public:
        explicit audiounit_ios_audio_driver(const std::uint32_t initial_master_volume = 100,
            const player_type preferred_midi_backend = player_type_tsf);
        ~audiounit_ios_audio_driver() override;

        std::unique_ptr<audio_output_stream> new_output_stream(const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback) override;

        std::unique_ptr<audio_input_stream> new_input_stream(const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback) override;

        std::uint32_t native_sample_rate() override;

        void suspend() override;
        void resume() override;

        void register_stream(audiounit_ios_stream_base *stream);
        void unregister_stream(audiounit_ios_stream_base *stream);

        bool activate_input_session();
        void deactivate_input_session();
    };
}
