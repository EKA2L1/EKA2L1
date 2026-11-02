/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <ldd/accelerometer/accelerometer.h>

#include <drivers/sensor/sensor.h>
#include <kernel/kernel.h>
#include <kernel/property.h>
#include <kernel/timing.h>
#include <system/epoc.h>

#include <common/log.h>
#include <utils/err.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace eka2l1::ldd {
    // The N95's LIS302DL reports about 18 mg per count, so 1 g is about 56 counts.
    static constexpr double ACCELEROMETER_COUNTS_PER_G = 56.0;
    static constexpr double STANDARD_GRAVITY = 9.80665;
    static constexpr std::int64_t ACCELEROMETER_SAMPLE_INTERVAL_US = 40000;

    accelerometer_stream::accelerometer_stream(kernel_system *kern, system *sys)
        : kern_(kern)
        , sys_(sys)
        , timing_(sys->get_ntimer())
        , sample_evt_(-1)
        , listener_count_(0) {
    }

    accelerometer_stream::~accelerometer_stream() {
        if (sample_evt_ >= 0) {
            timing_->unschedule_event(sample_evt_, 0);
            timing_->remove_event(sample_evt_);
        }
    }

    void accelerometer_stream::init() {
        std::weak_ptr<accelerometer_stream> weak_self = weak_from_this();

        sample_evt_ = timing_->register_event("AccelerometerSampleDue", [weak_self](std::uint64_t, int) {
            if (auto self = weak_self.lock()) {
                self->on_sample_due();
            }
        });
    }

    void accelerometer_stream::on_sample_due() {
        kern_->lock();

        if (!kern_->is_wiping() && listener_count_) {
            publish_sample();
            schedule_next_sample();
        }

        kern_->unlock();
    }

    void accelerometer_stream::schedule_next_sample() {
        timing_->schedule_event(ACCELEROMETER_SAMPLE_INTERVAL_US, sample_evt_, 0);
    }

    void accelerometer_stream::add_listener() {
        if (listener_count_++ == 0) {
            publish_sample();
            schedule_next_sample();
        }
    }

    void accelerometer_stream::remove_listener() {
        if (listener_count_ && (--listener_count_ == 0)) {
            timing_->unschedule_event(sample_evt_, 0);
        }
    }

    void accelerometer_stream::publish_sample() {
        // At rest, face up: only gravity, on the z axis.
        std::array<double, 3> acceleration{ 0.0, 0.0, STANDARD_GRAVITY };

        drivers::sensor_driver *driver = sys_->get_sensor_driver();
        if (driver) {
            if (const auto override_values = driver->acceleration_override()) {
                acceleration = *override_values;
            }
        }

        std::uint8_t sample[3];
        for (std::size_t i = 0; i < 3; i++) {
            const double counts = std::round(acceleration[i] / STANDARD_GRAVITY * ACCELEROMETER_COUNTS_PER_G);
            sample[i] = static_cast<std::uint8_t>(static_cast<std::int8_t>(std::clamp(counts, -128.0, 127.0)));
        }

        // A client that attached first leaves an undefined property behind.
        service::property *prop = kern_->get_prop(ACCELEROMETER_DATA_CATEGORY, ACCELEROMETER_DATA_KEY);
        if (!prop) {
            prop = kern_->create<service::property>();
            if (!prop) {
                LOG_ERROR(KERNEL, "Unable to create the accelerometer data property");
                return;
            }

            prop->first = ACCELEROMETER_DATA_CATEGORY;
            prop->second = ACCELEROMETER_DATA_KEY;
        }

        if (!prop->is_defined()) {
            prop->define(service::property_type::bin_data, sizeof(sample));
        }

        prop->set(sample, sizeof(sample));
    }

    accelerometer_factory::accelerometer_factory(kernel_system *kern, system *sys)
        : factory(kern, sys)
        , stream_(std::make_shared<accelerometer_stream>(kern, sys)) {
        stream_->init();
    }

    void accelerometer_factory::install() {
        obj_name = "accelerometer";
    }

    std::unique_ptr<channel> accelerometer_factory::make_channel(epoc::version ver) {
        return std::make_unique<accelerometer_channel>(kern, sys_, ver, stream_);
    }

    accelerometer_channel::accelerometer_channel(kernel_system *kern, system *sys, epoc::version ver,
        std::shared_ptr<accelerometer_stream> stream)
        : channel(kern, sys, ver)
        , stream_(std::move(stream))
        , started_(false) {
    }

    accelerometer_channel::~accelerometer_channel() {
        stop();
    }

    void accelerometer_channel::stop() {
        if (started_) {
            started_ = false;
            stream_->remove_listener();
        }
    }

    std::int32_t accelerometer_channel::do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
        const eka2l1::ptr<void> arg2) {
        switch (n) {
        case ACCELEROMETER_CONTROL_START:
            if (!started_) {
                started_ = true;
                stream_->add_listener();
            }

            return epoc::error_none;

        case ACCELEROMETER_CONTROL_STOP:
            stop();
            return epoc::error_none;

        default:
            LOG_WARN(KERNEL, "Unimplemented accelerometer control opcode {}", n);
            break;
        }

        return epoc::error_not_supported;
    }

    std::int32_t accelerometer_channel::do_request(epoc::notify_info info, const std::uint32_t n,
        const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
        const bool is_supervisor) {
        switch (n) {
        case ACCELEROMETER_REQUEST_CONFIGURE_SENSOR:
        case ACCELEROMETER_REQUEST_CONFIGURE_FILTER:
        case ACCELEROMETER_REQUEST_POWER_DOWN:
            // There is no hardware to program, so the request finishes at once.
            info.complete(epoc::error_none);
            break;

        default:
            LOG_WARN(KERNEL, "Unimplemented accelerometer request opcode {}", n);
            info.complete(epoc::error_not_supported);
            break;
        }

        return epoc::error_none;
    }
}
