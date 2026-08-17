/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>

#include <common/log.h>
#include <drivers/audio/audio.h>
#include <drivers/audio/backend/audiounit_ios/audio_audiounit_ios.h>
#include <drivers/audio/backend/audiounit_ios/stream_audiounit_ios.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>

namespace eka2l1::drivers {
    // Forward declaration so the file-local input render callback can ask
    // for the AudioUnit handle stored on the base class without exposing
    // ivars to that TU-local helper namespace.
    AudioUnit audiounit_ios_stream_handle(audiounit_ios_stream_base *);
}

namespace {

// CoreAudio invokes these render callbacks through a C ABI on its realtime
// I/O thread; some are reached via AudioConverterFillComplexBufferRealtimeSafe,
// which is not exception-aware. The guest callback chain below can legitimately
// throw (ffmpeg decode / std::vector growth / std::mutex acquisition / kernel
// completion), most notably during the last in-flight callback that
// AudioOutputUnitStop() drains while a stream is being torn down. Letting any
// exception escape into Apple's realtime code unwinds into std::terminate()
// (SIGABRT). Every callback body therefore runs inside a noexcept boundary and
// falls back to silence rather than crossing the C boundary with an exception.

OSStatus output_render_cb(void *inRefCon,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp * /*inTimeStamp*/,
                          UInt32 /*inBusNumber*/,
                          UInt32 inNumberFrames,
                          AudioBufferList *ioData) noexcept {
    auto *self = reinterpret_cast<eka2l1::drivers::audiounit_ios_stream_base *>(inRefCon);
    if (!self || !ioData || ioData->mNumberBuffers == 0) {
        return noErr;
    }
    AudioBuffer &buf = ioData->mBuffers[0];
    // S16 interleaved — buffer is sized for inNumberFrames * channels * 2.
    auto *out = reinterpret_cast<std::int16_t *>(buf.mData);
    std::size_t got = 0;
    try {
        got = self->call_callback(out, inNumberFrames);
    } catch (...) {
        got = 0;
    }
    const std::size_t produced_bytes = got * /*channels handled inside*/ sizeof(std::int16_t);
    (void)produced_bytes; // call_callback writes the right amount.
    if (got == 0) {
        std::memset(out, 0, buf.mDataByteSize);
        if (ioActionFlags) {
            *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
        }
    }
    return noErr;
}

OSStatus input_render_cb(void *inRefCon,
                         AudioUnitRenderActionFlags *ioActionFlags,
                         const AudioTimeStamp *inTimeStamp,
                         UInt32 inBusNumber,
                         UInt32 inNumberFrames,
                         AudioBufferList * /*ioData*/) noexcept {
    auto *self = reinterpret_cast<eka2l1::drivers::audiounit_ios_stream_base *>(inRefCon);
    if (!self) return noErr;

    // Pull captured frames from the unit, then hand them to the callback.
    // EKA2L1's data_callback uses the SAME pointer for input and output, so
    // we render into a scratch we own and pass it through.
    static thread_local std::vector<std::int16_t> scratch;
    const std::size_t channels = self->channel_count();
    const std::size_t needed = static_cast<std::size_t>(inNumberFrames) * channels;

    AudioUnit unit_handle = eka2l1::drivers::audiounit_ios_stream_handle(self);
    if (!unit_handle) return noErr;

    try {
        if (scratch.size() < needed) scratch.resize(needed);

        AudioBufferList list{};
        list.mNumberBuffers = 1;
        list.mBuffers[0].mNumberChannels = static_cast<UInt32>(channels);
        list.mBuffers[0].mDataByteSize = static_cast<UInt32>(needed * sizeof(std::int16_t));
        list.mBuffers[0].mData = scratch.data();

        OSStatus r = AudioUnitRender(unit_handle, ioActionFlags, inTimeStamp,
            inBusNumber, inNumberFrames, &list);
        if (r != noErr) return noErr;

        self->call_callback(scratch.data(), inNumberFrames);
    } catch (...) {
        // Swallow: never unwind across CoreAudio's C callback boundary.
    }
    return noErr;
}

} // namespace

namespace eka2l1::drivers {
    AudioUnit audiounit_ios_stream_handle(audiounit_ios_stream_base *);

    struct audiounit_ios_input_permission_state {
        std::mutex mutex;
        audiounit_ios_input_stream *stream = nullptr;
    };

    audiounit_ios_stream_base::audiounit_ios_stream_base(const std::uint32_t sample_rate,
        const std::uint8_t channels, data_callback callback, bool is_input)
        : callback_(std::move(callback))
        , sample_rate_(sample_rate)
        , channels_(channels)
        , is_input_(is_input) {
    }

    audiounit_ios_stream_base::~audiounit_ios_stream_base() {
        dispose_unit();
    }

    void audiounit_ios_stream_base::dispose_unit() {
        if (!unit_) {
            return;
        }
        if (running_.load()) {
            AudioOutputUnitStop(unit_);
            running_.store(false);
        }
        AudioUnitUninitialize(unit_);
        AudioComponentInstanceDispose(unit_);
        unit_ = nullptr;
    }

    std::size_t audiounit_ios_stream_base::call_callback(std::int16_t *buffer, const long frames) {
        if (should_idle()) {
            std::memset(buffer, 0, frames * channels_ * sizeof(std::int16_t));
            idle_frames_.fetch_add(static_cast<std::uint64_t>(frames),
                std::memory_order_relaxed);
            position_frames_.fetch_add(static_cast<std::uint64_t>(frames),
                std::memory_order_relaxed);
            return static_cast<std::size_t>(frames);
        }
        const std::size_t got = callback_ ? callback_(buffer, frames) : 0;
        if (got < static_cast<std::size_t>(frames)) {
            std::memset(buffer + got * channels_, 0,
                (frames - got) * channels_ * sizeof(std::int16_t));
        }
        position_frames_.fetch_add(static_cast<std::uint64_t>(frames),
            std::memory_order_relaxed);
        return static_cast<std::size_t>(frames);
    }

    bool audiounit_ios_stream_base::create_unit() {
        AudioComponentDescription desc{};
        desc.componentType = kAudioUnitType_Output;
        // RemoteIO is the iOS hardware I/O AudioUnit. The macOS-only HAL
        // / DefaultOutput types are deliberately not used here.
        desc.componentSubType = kAudioUnitSubType_RemoteIO;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
        if (!comp) {
            LOG_ERROR(DRIVER_AUD, "AudioComponentFindNext(RemoteIO) returned nullptr");
            return false;
        }
        if (AudioComponentInstanceNew(comp, &unit_) != noErr || !unit_) {
            LOG_ERROR(DRIVER_AUD, "AudioComponentInstanceNew(RemoteIO) failed");
            return false;
        }

        // RemoteIO bus numbering: bus 0 = output to speaker (element 0),
        // bus 1 = input from mic (element 1). Enable input on bus 1 for
        // recording, leave default (output-only) for playback.
        if (is_input_) {
            UInt32 enableIO = 1;
            OSStatus enable_result = AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Input, /*inputBus=*/1, &enableIO, sizeof(enableIO));
            if (enable_result != noErr) {
                LOG_ERROR(DRIVER_AUD, "AudioUnitSetProperty(EnableIO input) failed: {}",
                    enable_result);
                AudioComponentInstanceDispose(unit_);
                unit_ = nullptr;
                return false;
            }
            enableIO = 0;
            enable_result = AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output, /*outputBus=*/0, &enableIO, sizeof(enableIO));
            if (enable_result != noErr) {
                LOG_ERROR(DRIVER_AUD, "AudioUnitSetProperty(DisableIO output) failed: {}",
                    enable_result);
                AudioComponentInstanceDispose(unit_);
                unit_ = nullptr;
                return false;
            }
        }

        AudioStreamBasicDescription fmt{};
        fmt.mSampleRate = sample_rate_;
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
        fmt.mChannelsPerFrame = channels_;
        fmt.mBitsPerChannel = 16;
        fmt.mFramesPerPacket = 1;
        fmt.mBytesPerFrame = sizeof(std::int16_t) * channels_;
        fmt.mBytesPerPacket = fmt.mBytesPerFrame;

        // For output: stream format on bus 0 input scope (data we feed the
        // unit). For input: bus 1 output scope (data we read from the unit).
        // RemoteIO owns conversion between this client format and hardware.
        AudioUnitElement bus = is_input_ ? 1 : 0;
        AudioUnitScope scope = is_input_ ? kAudioUnitScope_Output : kAudioUnitScope_Input;
        OSStatus r = AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat,
            scope, bus, &fmt, sizeof(fmt));
        if (r != noErr) {
            LOG_ERROR(DRIVER_AUD, "AudioUnitSetProperty(StreamFormat) failed: {}", r);
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
            return false;
        }

        if (is_input_) {
            AURenderCallbackStruct cb{};
            cb.inputProc = input_render_cb;
            cb.inputProcRefCon = this;
            r = AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_SetInputCallback,
                kAudioUnitScope_Global, /*outputBus=*/0, &cb, sizeof(cb));
        } else {
            AURenderCallbackStruct cb{};
            cb.inputProc = output_render_cb;
            cb.inputProcRefCon = this;
            r = AudioUnitSetProperty(unit_, kAudioUnitProperty_SetRenderCallback,
                kAudioUnitScope_Input, /*bus=*/0, &cb, sizeof(cb));
        }
        if (r != noErr) {
            LOG_ERROR(DRIVER_AUD, "AudioUnitSetProperty(render callback) failed: {}", r);
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
            return false;
        }

        if (AudioUnitInitialize(unit_) != noErr) {
            LOG_ERROR(DRIVER_AUD, "AudioUnitInitialize failed");
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
            return false;
        }
        return true;
    }

    bool audiounit_ios_stream_base::start_unit() {
        if (!unit_ && !create_unit()) return false;
        if (running_.load()) return true;
        if (AudioOutputUnitStart(unit_) != noErr) {
            LOG_ERROR(DRIVER_AUD, "AudioOutputUnitStart failed");
            return false;
        }
        running_.store(true);
        return true;
    }

    bool audiounit_ios_stream_base::stop_unit() {
        if (!unit_ || !running_.load()) return true;
        if (AudioOutputUnitStop(unit_) != noErr) return false;
        running_.store(false);
        return true;
    }

    bool audiounit_ios_stream_base::restart_after_session_activation() {
        if (!unit_ || !running_.load()) {
            return true;
        }

        // running_ is the guest-visible intent and deliberately remains true:
        // this is only a host AudioUnit restart after AVAudioSession was
        // deactivated, not a stop requested by the emulated application.
        const OSStatus stop_result = AudioOutputUnitStop(unit_);
        if (stop_result != noErr) {
            LOG_WARN(DRIVER_AUD, "AudioOutputUnitStop before session resume failed: {}",
                stop_result);
        }
        const OSStatus start_result = AudioOutputUnitStart(unit_);
        if (start_result != noErr) {
            LOG_ERROR(DRIVER_AUD, "AudioOutputUnitStart after session resume failed: {}",
                start_result);
            return false;
        }
        return true;
    }

    // Implemented in the header via friend / accessor; placed here so the
    // input render callback can fetch the AudioUnit handle without exposing
    // ivars to TU-local helpers.
    AudioUnit audiounit_ios_stream_handle(audiounit_ios_stream_base *s) {
        struct unit_accessor : audiounit_ios_stream_base {
            using audiounit_ios_stream_base::unit_;
        };
        return s ? static_cast<unit_accessor *>(s)->unit_ : nullptr;
    }

    // ---- output stream ------------------------------------------------------

    audiounit_ios_output_stream::audiounit_ios_output_stream(audio_driver *driver,
        const std::uint32_t sample_rate, const std::uint8_t channels, data_callback callback)
        : audio_output_stream(driver, sample_rate, channels)
        , audiounit_ios_stream_base(sample_rate, channels, callback, /*is_input=*/false)
        , ios_driver_(static_cast<audiounit_ios_audio_driver *>(driver)) {
        ios_driver_->register_stream(this);
    }

    audiounit_ios_output_stream::~audiounit_ios_output_stream() {
        ios_driver_->unregister_stream(this);
        dispose_unit();
    }

    bool audiounit_ios_output_stream::should_idle() {
        return pausing_.load() || (driver_ && driver_->suspending());
    }

    bool audiounit_ios_output_stream::start() {
        pausing_.store(false);
        return start_unit();
    }

    bool audiounit_ios_output_stream::stop() {
        pausing_.store(false);
        return stop_unit();
    }

    void audiounit_ios_output_stream::pause() {
        pausing_.store(true);
    }

    bool audiounit_ios_output_stream::is_playing() { return running_.load(); }
    bool audiounit_ios_output_stream::is_pausing() { return pausing_.load(); }

    bool audiounit_ios_output_stream::set_volume(const float volume) {
        const float clamped = std::clamp(volume, 0.0f, 1.0f);
        volume_.store(clamped);
        // RemoteIO's kAudioUnitParameterUnit_LinearGain takes per-bus gain
        // on global scope element 0.
        if (unit_) {
            const std::uint32_t master = driver_ ? driver_->master_volume() : 100;
            const float effective = clamped * (static_cast<float>(master) / 100.0f);
            AudioUnitSetParameter(unit_, kAudioUnitParameterUnit_LinearGain,
                kAudioUnitScope_Global, 0, effective, 0);
        }
        return true;
    }

    float audiounit_ios_output_stream::get_volume() const { return volume_.load(); }

    bool audiounit_ios_output_stream::current_frame_position(std::uint64_t *pos) {
        if (!pos) return false;
        const std::uint64_t played = position_frames_.load(std::memory_order_relaxed);
        const std::uint64_t idle = idle_frames_.load(std::memory_order_relaxed);
        *pos = (played > idle) ? (played - idle) : 0;
        return true;
    }

    // ---- input stream -------------------------------------------------------

    audiounit_ios_input_stream::audiounit_ios_input_stream(audio_driver *driver,
        const std::uint32_t sample_rate, const std::uint8_t channels, data_callback callback)
        : audio_input_stream(driver, sample_rate, channels)
        , audiounit_ios_stream_base(sample_rate, channels, callback, /*is_input=*/true)
        , ios_driver_(static_cast<audiounit_ios_audio_driver *>(driver))
        , permission_state_(std::make_shared<audiounit_ios_input_permission_state>()) {
        permission_state_->stream = this;
        ios_driver_->register_stream(this);
    }

    audiounit_ios_input_stream::~audiounit_ios_input_stream() {
        start_requested_.store(false);
        {
            const std::lock_guard<std::mutex> guard(permission_state_->mutex);
            permission_state_->stream = nullptr;
        }
        {
            const std::lock_guard<std::mutex> guard(lifecycle_mutex_);
            stop_unit();
            if (input_session_active_) {
                ios_driver_->deactivate_input_session();
                input_session_active_ = false;
            }
        }
        ios_driver_->unregister_stream(this);
        dispose_unit();
    }

    bool audiounit_ios_input_stream::should_idle() {
        return driver_ && driver_->suspending();
    }

    bool audiounit_ios_input_stream::start_after_permission_granted() {
        const std::lock_guard<std::mutex> guard(lifecycle_mutex_);
        if (!start_requested_.load()) {
            return true;
        }
        if (running_.load()) {
            return true;
        }

        if (!input_session_active_ && !ios_driver_->activate_input_session()) {
            return false;
        }
        input_session_active_ = true;

        if (!start_unit()) {
            ios_driver_->deactivate_input_session();
            input_session_active_ = false;
            return false;
        }
        return true;
    }

    bool audiounit_ios_input_stream::start() {
        start_requested_.store(true);

        switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]) {
        case AVAuthorizationStatusAuthorized:
            return start_after_permission_granted();

        case AVAuthorizationStatusNotDetermined: {
            const std::shared_ptr<audiounit_ios_input_permission_state> state = permission_state_;
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                    completionHandler:^(BOOL granted) {
                const std::lock_guard<std::mutex> guard(state->mutex);
                if (!state->stream) {
                    return;
                }
                if (!granted) {
                    state->stream->start_requested_.store(false);
                    LOG_WARN(DRIVER_AUD, "Microphone access was denied");
                    return;
                }
                if (!state->stream->start_after_permission_granted()) {
                    LOG_ERROR(DRIVER_AUD, "Failed to start audio input after permission grant");
                }
            }];
            return true;
        }

        case AVAuthorizationStatusDenied:
        case AVAuthorizationStatusRestricted:
            start_requested_.store(false);
            LOG_WARN(DRIVER_AUD, "Microphone access is unavailable");
            return false;

        default:
            start_requested_.store(false);
            LOG_WARN(DRIVER_AUD, "Unknown microphone authorization status");
            return false;
        }
    }

    bool audiounit_ios_input_stream::stop() {
        start_requested_.store(false);
        const std::lock_guard<std::mutex> guard(lifecycle_mutex_);
        const bool stopped = stop_unit();
        // Route/category changes can alter RemoteIO's hardware side between
        // recording cycles. Recreate the unit next time instead of retaining
        // a converter initialized for the previous route.
        dispose_unit();
        if (input_session_active_) {
            ios_driver_->deactivate_input_session();
            input_session_active_ = false;
        }
        return stopped;
    }
    bool audiounit_ios_input_stream::is_recording() { return running_.load(); }

    bool audiounit_ios_input_stream::current_frame_position(std::uint64_t *pos) {
        if (!pos) return false;
        const std::uint64_t played = position_frames_.load(std::memory_order_relaxed);
        const std::uint64_t idle = idle_frames_.load(std::memory_order_relaxed);
        *pos = (played > idle) ? (played - idle) : 0;
        return true;
    }
}
