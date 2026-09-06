/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <drivers/hwrm/backend/vibration_ios.h>

#include <algorithm>
#include <cmath>
#include <mutex>

#import <CoreHaptics/CoreHaptics.h>
#import <GameController/GameController.h>

namespace eka2l1::drivers::hwrm {
    namespace {
        struct haptic_routing {
            std::mutex mutex;
            GCController *controller = nil;
            NSHashTable<CHHapticEngine *> *engines = [NSHashTable weakObjectsHashTable];
            std::uint64_t revision = 1;
            bool suspended = false;
        };

        haptic_routing &routing() {
            static haptic_routing state;
            return state;
        }

        void stop_engines_locked(haptic_routing &state) {
            for (CHHapticEngine *engine in state.engines) {
                [engine stopWithCompletionHandler:nil];
            }
            ++state.revision;
        }
    }

    void set_controller_haptic_source(void *controller) {
        auto &state = routing();
        const std::lock_guard<std::mutex> hold(state.mutex);
        GCController *source = (__bridge GCController *)controller;
        if (!source.haptics) source = nil;
        if (state.controller == source) return;
        stop_engines_locked(state);
        state.controller = source;
    }

    void set_vibration_suspended(bool suspended) {
        auto &state = routing();
        const std::lock_guard<std::mutex> hold(state.mutex);
        if (state.suspended == suspended) return;
        state.suspended = suspended;
        if (suspended) stop_engines_locked(state);
    }

    vibrator_ios::vibrator_ios() = default;

    vibrator_ios::~vibrator_ios() {
        const std::lock_guard<std::mutex> hold(routing().mutex);
        clear_engine_locked();
    }

    void vibrator_ios::stop_player_locked() {
        if (player_) {
            id<CHHapticPatternPlayer> player = (__bridge id<CHHapticPatternPlayer>)player_;
            [player stopAtTime:CHHapticTimeImmediate error:nil];
            CFRelease(player_);
            player_ = nullptr;
        }
    }

    void vibrator_ios::clear_engine_locked() {
        stop_player_locked();
        if (engine_) {
            CHHapticEngine *engine = (__bridge CHHapticEngine *)engine_;
            [engine stopWithCompletionHandler:nil];
            [routing().engines removeObject:engine];
            CFRelease(engine_);
            engine_ = nullptr;
        }
    }

    void vibrator_ios::vibrate(const std::uint32_t millisecs, const std::int16_t intensity) {
        auto &state = routing();
        const std::lock_guard<std::mutex> hold(state.mutex);
        stop_player_locked();
        if (state.suspended) return;
        if (source_revision_ != state.revision) clear_engine_locked();
        if (!engine_) {
            CHHapticEngine *engine = nil;
            if (state.controller) {
                engine = [state.controller.haptics createEngineWithLocality:GCHapticsLocalityDefault];
            } else if (CHHapticEngine.capabilitiesForHardware.supportsHaptics) {
                engine = [[CHHapticEngine alloc] initAndReturnError:nil];
            }
            if (!engine) return;
            engine.playsHapticsOnly = YES;
            engine_ = (__bridge_retained void *)engine;
            source_revision_ = state.revision;
            [state.engines addObject:engine];
        }

        CHHapticEngine *engine = (__bridge CHHapticEngine *)engine_;
        if (![engine startAndReturnError:nil]) return;
        // The driver API uses zero for default intensity; motor direction has no haptic equivalent.
        const float normalized = intensity == 0 ? 0.5f
            : std::min(1.0f, std::abs(static_cast<float>(intensity)) / 100.0f);
        const NSTimeInterval duration = millisecs == 0 ? 1.0 : static_cast<NSTimeInterval>(millisecs) / 1000.0;
        CHHapticEventParameter *intensity_param = [[CHHapticEventParameter alloc]
            initWithParameterID:CHHapticEventParameterIDHapticIntensity value:normalized];
        CHHapticEventParameter *sharpness_param = [[CHHapticEventParameter alloc]
            initWithParameterID:CHHapticEventParameterIDHapticSharpness value:0.45f];
        CHHapticEvent *event = [[CHHapticEvent alloc]
            initWithEventType:CHHapticEventTypeHapticContinuous
            parameters:@[ intensity_param, sharpness_param ] relativeTime:0.0 duration:duration];
        CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ] parameters:@[] error:nil];
        if (!pattern) return;
        id<CHHapticAdvancedPatternPlayer> player = [engine createAdvancedPlayerWithPattern:pattern error:nil];
        if (!player) return;
        if (millisecs == 0) {
            player.loopEnabled = YES;
            player.loopEnd = duration;
        }
        if ([player startAtTime:CHHapticTimeImmediate error:nil]) {
            player_ = (__bridge_retained void *)player;
        }
    }

    void vibrator_ios::stop_vibrate() {
        const std::lock_guard<std::mutex> hold(routing().mutex);
        stop_player_locked();
    }
}
