/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <drivers/audio/stream.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#ifdef __OBJC__
@class NSObject;
#endif

#if defined(__OBJC__)
#import <AudioToolbox/AudioToolbox.h>
#else
struct OpaqueAudioComponentInstance;
using AudioUnit = struct OpaqueAudioComponentInstance *;
#endif

namespace eka2l1::drivers {
    struct audiounit_ios_audio_driver;
    struct audiounit_ios_input_permission_state;

    // Common bits used by output and input AURemoteIO streams.
    struct audiounit_ios_stream_base {
        friend struct audiounit_ios_audio_driver;

    protected:
        AudioUnit unit_ = nullptr;
        data_callback callback_;

        std::uint32_t sample_rate_;
        std::uint8_t channels_;
        bool is_input_;

        std::atomic<bool> running_{false};
        std::atomic<std::uint64_t> position_frames_{0};
        std::atomic<std::uint64_t> idle_frames_{0};

    public:
        audiounit_ios_stream_base(const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback, bool is_input);
        virtual ~audiounit_ios_stream_base();

        std::size_t call_callback(std::int16_t *buffer, const long frames);
        std::uint8_t channel_count() const {
            return channels_;
        }

    protected:
        virtual bool should_idle() = 0;

        bool create_unit();
        bool start_unit();
        bool stop_unit();
        bool restart_after_session_activation();

        // Must be called from the MOST-DERIVED destructor: the render
        // callback invokes the virtual should_idle(), so the unit has to be
        // stopped (AudioOutputUnitStop is synchronous with in-flight
        // renders) while the vtable still points at the derived class.
        // Idempotent; the base destructor calls it again as a safety net.
        void dispose_unit();
    };

    struct audiounit_ios_output_stream final : public audio_output_stream,
                                               public audiounit_ios_stream_base {
    public:
        audiounit_ios_output_stream(audio_driver *driver, const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback);
        ~audiounit_ios_output_stream() override;

        bool start() override;
        bool stop() override;
        void pause() override;

        bool is_playing() override;
        bool is_pausing() override;

        bool set_volume(const float volume) override;
        float get_volume() const override;

        bool current_frame_position(std::uint64_t *pos) override;

    protected:
        bool should_idle() override;

    private:
        audiounit_ios_audio_driver *ios_driver_;
        std::atomic<bool> pausing_{false};
        std::atomic<float> volume_{1.0f};
    };

    struct audiounit_ios_input_stream final : public audio_input_stream,
                                              public audiounit_ios_stream_base {
    public:
        audiounit_ios_input_stream(audio_driver *driver, const std::uint32_t sample_rate,
            const std::uint8_t channels, data_callback callback);
        ~audiounit_ios_input_stream() override;

        bool start() override;
        bool stop() override;

        bool is_recording() override;
        bool current_frame_position(std::uint64_t *pos) override;

    protected:
        bool should_idle() override;

    private:
        bool start_after_permission_granted();

        audiounit_ios_audio_driver *ios_driver_;
        std::shared_ptr<audiounit_ios_input_permission_state> permission_state_;
        std::atomic<bool> start_requested_{false};
        std::mutex lifecycle_mutex_;
        bool input_session_active_ = false;
    };
}
