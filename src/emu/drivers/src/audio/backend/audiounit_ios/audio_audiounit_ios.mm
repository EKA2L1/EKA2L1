/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <AVFoundation/AVFoundation.h>

#include <common/log.h>
#include <drivers/audio/backend/audiounit_ios/audio_audiounit_ios.h>
#include <drivers/audio/backend/audiounit_ios/stream_audiounit_ios.h>
#include <drivers/audio/backend/baeplat_impl.h>

#include <algorithm>

namespace {

bool configure_playback_session() {
    NSError *error = nil;
    AVAudioSession *session = [AVAudioSession sharedInstance];
    if (![session setCategory:AVAudioSessionCategoryPlayback
                         mode:AVAudioSessionModeDefault
                      options:AVAudioSessionCategoryOptionMixWithOthers
                        error:&error]) {
        LOG_ERROR(eka2l1::DRIVER_AUD, "AVAudioSession Playback category failed: {}",
            error.localizedDescription.UTF8String ?: "unknown");
        return false;
    }

    error = nil;
    if (![session setActive:YES error:&error]) {
        LOG_ERROR(eka2l1::DRIVER_AUD, "AVAudioSession playback activation failed: {}",
            error.localizedDescription.UTF8String ?: "unknown");
        return false;
    }
    return true;
}

// AVAudioSession is a shared per-process singleton. Configure it on first
// driver instantiation; nothing tears it down explicitly (services come and
// go, the session category persists for the app's lifetime).
void configure_av_audio_session_once() {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        configure_playback_session();
    });
}

} // namespace

namespace eka2l1::drivers {
    audiounit_ios_audio_driver::audiounit_ios_audio_driver(const std::uint32_t initial_master_volume,
        const player_type preferred_midi_backend)
        : audio_driver(initial_master_volume, preferred_midi_backend) {
        configure_av_audio_session_once();
    }

    audiounit_ios_audio_driver::~audiounit_ios_audio_driver() {
        BAE_DriverDeactivated(this);
    }

    std::uint32_t audiounit_ios_audio_driver::native_sample_rate() {
        const double r = [AVAudioSession sharedInstance].sampleRate;
        if (r > 0.0) {
            return static_cast<std::uint32_t>(r);
        }
        return 48000;
    }

    void audiounit_ios_audio_driver::suspend() {
        audio_driver::suspend();
    }

    void audiounit_ios_audio_driver::resume() {
        if (!suspending()) {
            return;
        }

        // Reactivating AVAudioSession does not guarantee that RemoteIO units
        // interrupted by the deactivation begin rendering again. Keep the
        // guest-visible stream state intact, but explicitly restart every
        // live unit before the emulator threads leave their lifecycle pause.
        std::lock_guard<std::mutex> guard(streams_mutex_);
        for (audiounit_ios_stream_base *stream : streams_) {
            if (stream) {
                stream->restart_after_session_activation();
            }
        }
        audio_driver::resume();
    }

    void audiounit_ios_audio_driver::register_stream(audiounit_ios_stream_base *stream) {
        std::lock_guard<std::mutex> guard(streams_mutex_);
        streams_.push_back(stream);
    }

    void audiounit_ios_audio_driver::unregister_stream(audiounit_ios_stream_base *stream) {
        std::lock_guard<std::mutex> guard(streams_mutex_);
        streams_.erase(std::remove(streams_.begin(), streams_.end(), stream), streams_.end());
    }

    bool audiounit_ios_audio_driver::activate_input_session() {
        const std::lock_guard<std::mutex> guard(session_mutex_);
        if (active_input_sessions_ != 0) {
            ++active_input_sessions_;
            return true;
        }

        NSError *error = nil;
        AVAudioSession *session = [AVAudioSession sharedInstance];
        const AVAudioSessionCategoryOptions options =
            AVAudioSessionCategoryOptionMixWithOthers |
            AVAudioSessionCategoryOptionAllowBluetoothHFP |
            AVAudioSessionCategoryOptionAllowBluetoothA2DP;
        if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                              mode:AVAudioSessionModeDefault
                           options:options
                             error:&error]) {
            LOG_ERROR(DRIVER_AUD, "AVAudioSession PlayAndRecord category failed: {}",
                error.localizedDescription.UTF8String ?: "unknown");
            return false;
        }

        error = nil;
        if (![session setActive:YES error:&error]) {
            LOG_ERROR(DRIVER_AUD, "AVAudioSession input activation failed: {}",
                error.localizedDescription.UTF8String ?: "unknown");
            configure_playback_session();
            return false;
        }

        active_input_sessions_ = 1;
        return true;
    }

    void audiounit_ios_audio_driver::deactivate_input_session() {
        const std::lock_guard<std::mutex> guard(session_mutex_);
        if (active_input_sessions_ == 0 || --active_input_sessions_ != 0) {
            return;
        }

        // Playback is the emulator's steady state. Restoring it after the
        // final recorder stops follows the user's selected output route.
        configure_playback_session();
    }

    std::unique_ptr<audio_output_stream> audiounit_ios_audio_driver::new_output_stream(
        const std::uint32_t sample_rate, const std::uint8_t channels, data_callback callback) {
        return std::unique_ptr<audio_output_stream>(
            new audiounit_ios_output_stream(this, sample_rate, channels, callback));
    }

    std::unique_ptr<audio_input_stream> audiounit_ios_audio_driver::new_input_stream(
        const std::uint32_t sample_rate, const std::uint8_t channels, data_callback callback) {
        return std::unique_ptr<audio_input_stream>(
            new audiounit_ios_input_stream(this, sample_rate, channels, callback));
    }
}
