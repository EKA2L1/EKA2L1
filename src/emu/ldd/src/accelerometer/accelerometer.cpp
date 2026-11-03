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
#include <cstring>

namespace eka2l1::ldd {
    // The N95's LIS302DL reports about 18 mg per count, so 1 g is about 56 counts.
    static constexpr double ACCELEROMETER_COUNTS_PER_G = 56.0;
    static constexpr double STANDARD_GRAVITY = 9.80665;
    static constexpr std::int64_t ACCELEROMETER_SAMPLE_INTERVAL_US = 40000;

    // Host axes point right, toward the top, and out of the screen (face up reads +z).
    // The N95 raw frame is rotated 180 degrees about y, reversing x and z.
    static constexpr std::array<double, 3> N95_AXIS_SIGN{ -1.0, 1.0, -1.0 };

    // Host backends scale samples so that the active measure range maps to this value.
    static constexpr double HOST_SCALED_RANGE_MAX = 127.0;
    static constexpr double DEFAULT_HOST_MEASURE_RANGE_MS2 = 2.0 * STANDARD_GRAVITY;

    accelerometer_stream::accelerometer_stream(kernel_system *kern, system *sys)
        : kern_(kern)
        , sys_(sys)
        , timing_(sys->get_ntimer())
        , sample_evt_(-1)
        , rearm_evt_(-1)
        , listener_count_(0)
        , host_measure_range_ms2_(DEFAULT_HOST_MEASURE_RANGE_MS2)
        , host_generation_(0) {
    }

    accelerometer_stream::~accelerometer_stream() {
        if (sample_evt_ >= 0) {
            timing_->unschedule_event(sample_evt_, 0);
            timing_->remove_event(sample_evt_);
        }

        if (rearm_evt_ >= 0) {
            timing_->unschedule_event(rearm_evt_, 0);
            timing_->remove_event(rearm_evt_);
        }

        close_host_sensor();
    }

    void accelerometer_stream::init() {
        std::weak_ptr<accelerometer_stream> weak_self = weak_from_this();

        sample_evt_ = timing_->register_event("AccelerometerSampleDue", [weak_self](std::uint64_t, int) {
            if (auto self = weak_self.lock()) {
                self->on_sample_due();
            }
        });

        rearm_evt_ = timing_->register_event("AccelerometerHostRearm", [weak_self](std::uint64_t, int) {
            if (auto self = weak_self.lock()) {
                self->request_host_sample();
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
            open_host_sensor();
            publish_sample();
            schedule_next_sample();
        }
    }

    void accelerometer_stream::remove_listener() {
        if (listener_count_ && (--listener_count_ == 0)) {
            timing_->unschedule_event(sample_evt_, 0);
            close_host_sensor();
        }
    }

    void accelerometer_stream::open_host_sensor() {
        drivers::sensor_driver *driver = sys_->get_sensor_driver();
        if (!driver) {
            return;
        }

        drivers::sensor_info search_info;
        search_info.type_ = drivers::SENSOR_TYPE_ACCELEROMETER;
        search_info.data_type_ = drivers::SENSOR_DATA_TYPE_ACCELOREMETER_AXIS;

        const std::vector<drivers::sensor_info> infos = driver->queries_active_sensor(search_info);
        if (infos.empty()) {
            return;
        }

        std::unique_ptr<drivers::sensor> host_sensor = driver->new_sensor_controller(infos[0].id_);
        if (!host_sensor || (host_sensor->data_packet_size() != sizeof(drivers::sensor_accelerometer_axis_data))) {
            return;
        }

        // The measure range is an array property; its status selects the active element.
        double measure_range = DEFAULT_HOST_MEASURE_RANGE_MS2;
        drivers::sensor_property_data range_status{};
        range_status.data_type_ = drivers::sensor_property_data::DATA_TYPE_BUFFER;

        if (host_sensor->get_property(drivers::SENSOR_PROPERTY_MEASURE_RANGE, -1, drivers::SENSOR_PROPERTY_SINGLE, range_status)
            && (range_status.data_type_ == drivers::sensor_property_data::DATA_TYPE_INT)
            && (range_status.array_index_ == drivers::SENSOR_PROPERTY_ARRAY) && (range_status.int_value_ >= 0)) {
            drivers::sensor_property_data range{};
            range.data_type_ = drivers::sensor_property_data::DATA_TYPE_BUFFER;

            if (host_sensor->get_property(drivers::SENSOR_PROPERTY_MEASURE_RANGE, -1, range_status.int_value_, range)
                && (range.data_type_ == drivers::sensor_property_data::DATA_TYPE_DOUBLE)
                && (range.max_float_value_ >= STANDARD_GRAVITY) && (range.max_float_value_ <= 16.0 * STANDARD_GRAVITY)) {
                measure_range = range.max_float_value_;
            }
        }

        if (!host_sensor->listen_for_data(1, 1, 0)) {
            LOG_WARN(KERNEL, "Unable to listen to the host accelerometer");
            return;
        }

        {
            const std::lock_guard<std::mutex> guard(host_sensor_lock_);
            host_sensor_ = std::move(host_sensor);
            host_measure_range_ms2_ = measure_range;
        }

        request_host_sample();
    }

    void accelerometer_stream::close_host_sensor() {
        std::unique_ptr<drivers::sensor> host_sensor;

        {
            const std::lock_guard<std::mutex> guard(host_sensor_lock_);
            host_sensor = std::move(host_sensor_);
        }

        if (host_sensor) {
            host_sensor->cancel_data_listening();
        }

        const std::lock_guard<std::mutex> guard(host_sample_lock_);
        host_sample_.reset();
        host_generation_++;
    }

    void accelerometer_stream::request_host_sample() {
        const std::lock_guard<std::mutex> guard(host_sensor_lock_);
        if (!host_sensor_) {
            return;
        }

        std::uint64_t generation = 0;
        {
            const std::lock_guard<std::mutex> sample_guard(host_sample_lock_);
            generation = host_generation_;
        }

        const double scale = host_measure_range_ms2_ / HOST_SCALED_RANGE_MAX;
        std::weak_ptr<accelerometer_stream> weak_self = weak_from_this();

        host_sensor_->receive_data([weak_self, generation, scale](std::vector<std::uint8_t> &data, std::size_t packet_count) {
            if (auto self = weak_self.lock()) {
                self->on_host_data(data, packet_count, generation, scale);
            }
        });
    }

    void accelerometer_stream::on_host_data(const std::vector<std::uint8_t> &data, const std::size_t packet_count,
        const std::uint64_t generation, const double scale) {
        // Runs on the backend's thread with its lock held, so the next request is
        // made from the timer thread instead of from here.
        const std::size_t packet_size = sizeof(drivers::sensor_accelerometer_axis_data);
        const std::size_t available = std::min(packet_count, data.size() / packet_size);

        if (available) {
            drivers::sensor_accelerometer_axis_data axis_data;
            std::memcpy(&axis_data, data.data() + (available - 1) * packet_size, packet_size);

            const std::lock_guard<std::mutex> guard(host_sample_lock_);
            if (generation != host_generation_) {
                return;
            }

            host_sample_ = std::array<double, 3>{ axis_data.axis_x_ * scale, axis_data.axis_y_ * scale,
                axis_data.axis_z_ * scale };
        }

        timing_->schedule_event(0, rearm_evt_, 0);
    }

    void accelerometer_stream::publish_sample() {
        // At rest, face up: only gravity, on the z axis.
        std::array<double, 3> acceleration{ 0.0, 0.0, STANDARD_GRAVITY };

        std::optional<std::array<double, 3>> host_sample;
        {
            const std::lock_guard<std::mutex> guard(host_sample_lock_);
            host_sample = host_sample_;
        }

        if (host_sample) {
            acceleration = *host_sample;
        } else if (drivers::sensor_driver *driver = sys_->get_sensor_driver()) {
            // Backends that deliver samples apply the override themselves.
            if (const auto override_values = driver->acceleration_override()) {
                acceleration = *override_values;
            }
        }

        std::uint8_t sample[3];
        for (std::size_t i = 0; i < 3; i++) {
            const double counts = std::round(N95_AXIS_SIGN[i] * acceleration[i] / STANDARD_GRAVITY * ACCELEROMETER_COUNTS_PER_G);
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
