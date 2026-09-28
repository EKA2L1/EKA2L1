/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#import <CoreLocation/CoreLocation.h>

#include "location_ios.h"

#include <cmath>
#include <limits>
#include <mutex>

@class EKA2L1LocationDelegate;

namespace eka2l1::drivers {
    // The manager and delegate are only touched on the main queue, which owns their run loop.
    struct location_ios_state {
        std::mutex lock_;
        location_update_callback callback_;

        CLLocationManager *manager_ = nil;
        EKA2L1LocationDelegate *delegate_ = nil;
        bool running_ = false;
    };
}

using eka2l1::drivers::location_ios_state;

static float value_or_nan(const double value, const bool valid) {
    return valid ? static_cast<float>(value) : std::numeric_limits<float>::quiet_NaN();
}

@interface EKA2L1LocationDelegate : NSObject <CLLocationManagerDelegate>
- (instancetype)initWithState:(location_ios_state *)state;
@end

@implementation EKA2L1LocationDelegate {
    location_ios_state *state_;
}

- (instancetype)initWithState:(location_ios_state *)state {
    if (self = [super init]) {
        state_ = state;
    }

    return self;
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager {
    // Updates requested before the user answered the prompt do not start by themselves.
    const CLAuthorizationStatus status = manager.authorizationStatus;
    if (state_->running_ && ((status == kCLAuthorizationStatusAuthorizedWhenInUse) || (status == kCLAuthorizationStatusAuthorizedAlways))) {
        [manager startUpdatingLocation];
    }
}

- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray<CLLocation *> *)locations {
    CLLocation *location = locations.lastObject;
    if (!location || (location.horizontalAccuracy < 0)) {
        return;
    }

    eka2l1::drivers::location_fix fix;
    fix.latitude_ = location.coordinate.latitude;
    fix.longitude_ = location.coordinate.longitude;
    fix.horizontal_accuracy_ = static_cast<float>(location.horizontalAccuracy);
    fix.altitude_ = value_or_nan(location.altitude, location.verticalAccuracy >= 0);
    fix.vertical_accuracy_ = value_or_nan(location.verticalAccuracy, location.verticalAccuracy >= 0);
    fix.speed_ = value_or_nan(location.speed, location.speed >= 0);
    fix.speed_accuracy_ = value_or_nan(location.speedAccuracy, location.speedAccuracy >= 0);
    fix.course_ = value_or_nan(location.course, location.course >= 0);
    fix.course_accuracy_ = value_or_nan(location.courseAccuracy, location.courseAccuracy >= 0);

    const std::lock_guard<std::mutex> guard(state_->lock_);
    if (state_->callback_) {
        state_->callback_(fix);
    }
}

@end

namespace eka2l1::drivers {
    location_driver_ios::location_driver_ios()
        : state_(std::make_shared<location_ios_state>()) {
    }

    location_driver_ios::~location_driver_ios() {
        stop();

        // CLLocationManager must be released on the thread that created it.
        std::shared_ptr<location_ios_state> state = state_;
        dispatch_async(dispatch_get_main_queue(), ^{
            state->manager_.delegate = nil;
            state->manager_ = nil;
            state->delegate_ = nil;
        });
    }

    void location_driver_ios::start(location_update_callback callback) {
        {
            const std::lock_guard<std::mutex> guard(state_->lock_);
            state_->callback_ = std::move(callback);
        }

        std::shared_ptr<location_ios_state> state = state_;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!state->manager_) {
                state->delegate_ = [[EKA2L1LocationDelegate alloc] initWithState:state.get()];
                state->manager_ = [[CLLocationManager alloc] init];
                state->manager_.desiredAccuracy = kCLLocationAccuracyBest;
                state->manager_.distanceFilter = kCLDistanceFilterNone;
                state->manager_.delegate = state->delegate_;
            }

            state->running_ = true;

            if (state->manager_.authorizationStatus == kCLAuthorizationStatusNotDetermined) {
                [state->manager_ requestWhenInUseAuthorization];
            }

            [state->manager_ startUpdatingLocation];
        });
    }

    void location_driver_ios::stop() {
        {
            const std::lock_guard<std::mutex> guard(state_->lock_);
            state_->callback_ = nullptr;
        }

        std::shared_ptr<location_ios_state> state = state_;
        dispatch_async(dispatch_get_main_queue(), ^{
            state->running_ = false;
            [state->manager_ stopUpdatingLocation];
        });
    }
}
