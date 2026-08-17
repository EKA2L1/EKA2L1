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

#import <CoreHaptics/CoreHaptics.h>

namespace eka2l1::drivers::hwrm {
    vibrator_ios::vibrator_ios() {
        if (!CHHapticEngine.capabilitiesForHardware.supportsHaptics) {
            return;
        }

        NSError *error = nil;
        CHHapticEngine *engine = [[CHHapticEngine alloc] initAndReturnError:&error];
        if (error || !engine) {
            return;
        }

        [engine startAndReturnError:nil];
        engine_ = (__bridge_retained void *)engine;
    }

    vibrator_ios::~vibrator_ios() {
        stop_vibrate();
        if (engine_) {
            CFRelease(engine_);
            engine_ = nullptr;
        }
    }

    void vibrator_ios::vibrate(const std::uint32_t millisecs, const std::int16_t intensity) {
        if (!engine_) {
            return;
        }

        CHHapticEngine *engine = (__bridge CHHapticEngine *)engine_;
        const float clamped = std::clamp(static_cast<float>(intensity), -100.0f, 100.0f);
        const float normalized = std::max(0.15f, (clamped + 100.0f) / 200.0f);
        const NSTimeInterval duration = std::max<NSTimeInterval>(0.03, static_cast<NSTimeInterval>(millisecs) / 1000.0);

        CHHapticEventParameter *intensity_param = [[CHHapticEventParameter alloc]
            initWithParameterID:CHHapticEventParameterIDHapticIntensity value:normalized];
        CHHapticEventParameter *sharpness_param = [[CHHapticEventParameter alloc]
            initWithParameterID:CHHapticEventParameterIDHapticSharpness value:0.45f];
        CHHapticEvent *event = [[CHHapticEvent alloc]
            initWithEventType:CHHapticEventTypeHapticContinuous
            parameters:@[ intensity_param, sharpness_param ]
            relativeTime:0.0
            duration:duration];

        NSError *error = nil;
        CHHapticPattern *pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ] parameters:@[] error:&error];
        if (error || !pattern) {
            return;
        }

        id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
        if (error || !player) {
            return;
        }

        [engine startAndReturnError:nil];
        [player startAtTime:CHHapticTimeImmediate error:nil];
    }

    void vibrator_ios::stop_vibrate() {
        if (!engine_) {
            return;
        }

        CHHapticEngine *engine = (__bridge CHHapticEngine *)engine_;
        [engine stopWithCompletionHandler:nil];
    }
}
