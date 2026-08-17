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

#import <CoreMotion/CoreMotion.h>

#include "sensor_ios.h"

#include <common/log.h>
#include <common/time.h>

#include <algorithm>

namespace eka2l1::drivers {
    // Channel model mirrors the Android backend (values taken from a Nokia
    // 5800 XpressMusic): scaled S8 axis data over a selectable measure range.
    static constexpr std::int32_t ACCELEROMETER_SCALE_RANGE_MAX = 127;
    static constexpr std::int32_t ACCELEROMETER_SCALE_RANGE_MIN = -128;
    static const double ACCELEROMETER_MEASURE_RANGE_AVAILABLE[] = {
        19.62, 78.48
    };
    static const std::int32_t SAMPLING_RATE_AVAILABLE[] = {
        10, 40, 50
    };
    static const std::int32_t ACCELEROMETER_MEASURE_RANGE_MAX_OPTION = sizeof(ACCELEROMETER_MEASURE_RANGE_AVAILABLE) / sizeof(double);
    static const std::int32_t SAMPLING_RATE_MAX_OPTION = sizeof(SAMPLING_RATE_AVAILABLE) / sizeof(std::int32_t);

    static constexpr double STANDARD_GRAVITY_MS2 = 9.80665;
    static constexpr std::uint32_t ACCELEROMETER_SENSOR_ID = 1;

    // The CoreMotion handler runs on its own NSOperationQueue thread and may
    // already be queued when the driver goes away, so it only reaches the
    // driver through this guard: the driver's destructor (and the pause
    // barrier) takes the lock and clears/blocks on it.
    struct core_motion_owner_guard {
        std::mutex lock_;
        sensor_driver_ios *owner_ = nullptr;
    };

    struct core_motion_pump {
        CMMotionManager *manager_ = nil;
        NSOperationQueue *queue_ = nil;
        bool running_ = false;
        std::uint32_t running_rate_hz_ = 0;
        std::shared_ptr<core_motion_owner_guard> guard_;
    };

    // ---- sensor_ios ---------------------------------------------------------

    sensor_ios::sensor_ios(sensor_driver_ios *driver)
        : driver_(driver)
        , listening_(false)
        , packets_wanted_(1)
        , packets_buffered_(0)
        , data_callback_(nullptr)
        , active_accel_measure_range_(0)
        , active_sampling_rate_(1) {
    }

    sensor_ios::~sensor_ios() {
        cancel_data_listening();
    }

    bool sensor_ios::get_property(const sensor_property prop, const std::int32_t item_index,
        const std::int32_t array_index, sensor_property_data &data) {
        data.property_id_ = prop;

        switch (prop) {
        case SENSOR_PROPERTY_SAMPLE_RATE:
            if (array_index == SENSOR_PROPERTY_ARRAY) {
                data.set_as_array_status(sensor_property_data::DATA_TYPE_INT, SAMPLING_RATE_MAX_OPTION - 1,
                    active_sampling_rate_);
            } else {
                if ((array_index >= SAMPLING_RATE_MAX_OPTION) || (array_index < 0)) {
                    LOG_ERROR(SERVICE_SENSOR, "Trying to get out-of-bound sample rate!");
                    return false;
                }

                data.set_int(SAMPLING_RATE_AVAILABLE[array_index]);
                data.array_index_ = array_index;
            }

            break;

        case SENSOR_PROPERTY_DATA_FORMAT:
            data.set_int(SENSOR_DATA_FORMAT_SCALED);
            break;

        case SENSOR_PROPERTY_SCALED_RANGE:
            data.set_int_range(ACCELEROMETER_SCALE_RANGE_MIN, ACCELEROMETER_SCALE_RANGE_MAX);
            break;

        case SENSOR_PROPERTY_CHANNEL_UNIT:
            data.set_int(SENSOR_UNIT_MS_PER_S2);
            break;

        case SENSOR_PROPERTY_SCALE:
            data.set_int(0);
            break;

        case SENSOR_PROPERTY_MEASURE_RANGE:
            if (array_index == SENSOR_PROPERTY_ARRAY) {
                data.set_as_array_status(sensor_property_data::DATA_TYPE_DOUBLE, ACCELEROMETER_MEASURE_RANGE_MAX_OPTION - 1,
                    active_accel_measure_range_);
            } else {
                if ((array_index >= ACCELEROMETER_MEASURE_RANGE_MAX_OPTION) || (array_index < 0)) {
                    LOG_ERROR(SERVICE_SENSOR, "Trying to get out-of-bound measure range!");
                    return false;
                }

                data.set_double_range(-ACCELEROMETER_MEASURE_RANGE_AVAILABLE[array_index],
                    ACCELEROMETER_MEASURE_RANGE_AVAILABLE[array_index]);
                data.array_index_ = array_index;
            }

            break;

        default:
            LOG_TRACE(SERVICE_SENSOR, "Unhandled getting accelerometer sensor property {}", static_cast<int>(prop));
            break;
        }

        return true;
    }

    bool sensor_ios::set_property(const sensor_property_data &data) {
        switch (data.property_id_) {
        case SENSOR_PROPERTY_SAMPLE_RATE:
            if (data.array_index_ == SENSOR_PROPERTY_ARRAY) {
                if ((data.int_value_ >= SAMPLING_RATE_MAX_OPTION) || (data.int_value_ < 0)) {
                    LOG_ERROR(SERVICE_SENSOR, "Trying to set out-of-bound sample rate!");
                    return false;
                }

                active_sampling_rate_ = static_cast<std::uint32_t>(data.int_value_);
                driver_->refresh_pump();
                return true;
            }

            LOG_ERROR(SERVICE_SENSOR, "Trying to set read-only sample rate property! Only current sample rate index can be set!");
            return false;

        case SENSOR_PROPERTY_MEASURE_RANGE:
            if (data.array_index_ == SENSOR_PROPERTY_ARRAY) {
                if ((data.int_value_ >= ACCELEROMETER_MEASURE_RANGE_MAX_OPTION) || (data.int_value_ < 0)) {
                    LOG_ERROR(SERVICE_SENSOR, "Trying to set out-of-bound measure range!");
                    return false;
                }

                active_accel_measure_range_ = static_cast<std::uint32_t>(data.int_value_);
                return true;
            }

            LOG_ERROR(SERVICE_SENSOR, "Trying to set read-only measure range property! Only current measure range index can be set!");
            return false;

        default:
            LOG_TRACE(SERVICE_SENSOR, "Unhandled setting accelerometer sensor property {}", static_cast<int>(data.property_id_));
            break;
        }

        return false;
    }

    void sensor_ios::push_sample(const double x_ms2, const double y_ms2, const double z_ms2) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (!data_callback_) {
            return;
        }

        const double measure_range = ACCELEROMETER_MEASURE_RANGE_AVAILABLE[active_accel_measure_range_];
        auto scale = [measure_range](const double value_ms2) -> std::int32_t {
            const double scaled = value_ms2 * ACCELEROMETER_SCALE_RANGE_MAX / measure_range;
            return static_cast<std::int32_t>(std::clamp<double>(scaled, ACCELEROMETER_SCALE_RANGE_MIN,
                ACCELEROMETER_SCALE_RANGE_MAX));
        };

        sensor_accelerometer_axis_data axis_data;
        axis_data.timestamp_ = common::get_current_utc_time_in_microseconds_since_0ad();
        axis_data.axis_x_ = scale(x_ms2);
        axis_data.axis_y_ = scale(y_ms2);
        axis_data.axis_z_ = scale(z_ms2);

        events_translated_.insert(events_translated_.end(), reinterpret_cast<std::uint8_t *>(&axis_data),
            reinterpret_cast<std::uint8_t *>(&axis_data + 1));

        packets_buffered_++;
    }

    bool sensor_ios::take_ready_batch(sensor_data_callback &callback_out, std::vector<std::uint8_t> &data_out,
        std::size_t &packet_count_out) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (!data_callback_ || (packets_buffered_ < packets_wanted_)) {
            return false;
        }

        callback_out = data_callback_;
        data_out = std::move(events_translated_);
        packet_count_out = packets_buffered_;

        data_callback_ = nullptr;
        events_translated_.clear();
        packets_buffered_ = 0;

        return true;
    }

    void sensor_ios::receive_data(sensor_data_callback callback) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (data_callback_) {
            return;
        }

        packets_buffered_ = 0;
        data_callback_ = callback;
        events_translated_.clear();
    }

    bool sensor_ios::listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) {
        if (listening_) {
            return false;
        }

        if (desired_buffering_count > max_buffering_count) {
            LOG_ERROR(DRIVER_SENSOR, "Desired buffering count is bigger then max buffering count!");
            return false;
        }

        if (desired_buffering_count == 0) {
            desired_buffering_count = max_buffering_count;
        }

        if (desired_buffering_count == 0) {
            desired_buffering_count = 1;
        }

        {
            const std::lock_guard<std::mutex> guard(lock_);
            packets_wanted_ = desired_buffering_count;
            packets_buffered_ = 0;
            events_translated_.clear();
        }

        listening_ = true;
        driver_->track_active_listener(&listening_link_);

        return true;
    }

    bool sensor_ios::cancel_data_listening() {
        const bool was_listening = listening_;

        if (was_listening) {
            listening_ = false;
            driver_->untrack_active_listener(&listening_link_);
        }

        // A request that has not yet been moved into dispatch_sample's local
        // ready list must not retain the service callback after cancellation.
        const std::lock_guard<std::mutex> guard(lock_);
        data_callback_ = nullptr;
        events_translated_.clear();
        packets_buffered_ = 0;

        return was_listening;
    }

    std::vector<sensor_property_data> sensor_ios::get_all_properties(const sensor_property *prop_value) {
        LOG_ERROR(SERVICE_SENSOR, "Get all properties unimplemented!");
        return std::vector<sensor_property_data>{};
    }

    // ---- sensor_driver_ios --------------------------------------------------

    sensor_driver_ios::sensor_driver_ios()
        : pump_(std::make_unique<core_motion_pump>())
        , paused_(false)
        , motion_rotation_deg_(0) {
        pump_->manager_ = [[CMMotionManager alloc] init];
        pump_->queue_ = [[NSOperationQueue alloc] init];
        pump_->queue_.maxConcurrentOperationCount = 1;
        pump_->queue_.name = @"com.eka2l1.emulator.sensor";
        pump_->guard_ = std::make_shared<core_motion_owner_guard>();
        pump_->guard_->owner_ = this;
    }

    sensor_driver_ios::~sensor_driver_ios() {
        {
            const std::lock_guard<std::mutex> hold(list_lock_);
            if (pump_->running_) {
                [pump_->manager_ stopAccelerometerUpdates];
                pump_->running_ = false;
            }
        }

        // Barrier: a handler block already dequeued keeps the guard alive via
        // shared_ptr; once owner_ is cleared under the lock it can no longer
        // reach this object.
        const std::lock_guard<std::mutex> hold(pump_->guard_->lock_);
        pump_->guard_->owner_ = nullptr;
    }

    bool sensor_driver_ios::accelerometer_available() const {
        return pump_->manager_.accelerometerAvailable;
    }

    std::uint32_t sensor_driver_ios::max_requested_sampling_rate_locked() {
        std::uint32_t best = SAMPLING_RATE_AVAILABLE[0];

        if (!listening_list_.empty()) {
            common::double_linked_queue_element *first = listening_list_.first();
            common::double_linked_queue_element *end = listening_list_.end();

            do {
                sensor_ios *sensor_obj = E_LOFF(first, sensor_ios, listening_link_);
                const std::uint32_t idx = std::min<std::uint32_t>(sensor_obj->active_sampling_rate_,
                    SAMPLING_RATE_MAX_OPTION - 1);
                best = std::max<std::uint32_t>(best, SAMPLING_RATE_AVAILABLE[idx]);

                first = first->next;
            } while (first != end);
        }

        return best;
    }

    void sensor_driver_ios::refresh_pump_locked() {
        const bool want_running = !paused_ && !listening_list_.empty();

        if (!want_running) {
            if (pump_->running_) {
                [pump_->manager_ stopAccelerometerUpdates];
                pump_->running_ = false;
                pump_->running_rate_hz_ = 0;
            }
            return;
        }

        const std::uint32_t rate_hz = max_requested_sampling_rate_locked();
        pump_->manager_.accelerometerUpdateInterval = 1.0 / static_cast<double>(rate_hz);

        if (pump_->running_) {
            pump_->running_rate_hz_ = rate_hz;
            return;
        }

        std::shared_ptr<core_motion_owner_guard> guard = pump_->guard_;
        [pump_->manager_ startAccelerometerUpdatesToQueue:pump_->queue_
                                              withHandler:^(CMAccelerometerData *data, NSError *error) {
            if (!data) {
                return;
            }

            const std::lock_guard<std::mutex> hold(guard->lock_);
            if (!guard->owner_) {
                return;
            }

            // CoreMotion reports in g with gravity measured along the negative
            // axis (flat, screen up: z ~= -1). Symbian/Android channels report
            // the reaction force in m/s^2 (flat, screen up: z ~= +9.81) with
            // the same axis orientation — negate and rescale.
            guard->owner_->dispatch_sample(
                -data.acceleration.x * STANDARD_GRAVITY_MS2,
                -data.acceleration.y * STANDARD_GRAVITY_MS2,
                -data.acceleration.z * STANDARD_GRAVITY_MS2);
        }];

        pump_->running_ = true;
        pump_->running_rate_hz_ = rate_hz;
    }

    void sensor_driver_ios::refresh_pump() {
        const std::lock_guard<std::mutex> hold(list_lock_);
        refresh_pump_locked();
    }

    void sensor_driver_ios::set_motion_rotation(const int degrees) {
        motion_rotation_deg_.store(((degrees % 360) + 360) % 360, std::memory_order_relaxed);
    }

    void sensor_driver_ios::dispatch_sample(const double x_raw_ms2, const double y_raw_ms2, const double z_ms2) {
        // CoreMotion reports in the iPhone's fixed frame, but the guest reads
        // the emulated device's frame — the two coincide only when the player
        // holds the iPhone the way the Symbian phone would be held for the
        // current guest screen mode. Rotate X/Y into the emulated frame
        // (components of a vector in a frame rotated CCW by θ pick up R(-θ)).
        double x_ms2 = x_raw_ms2;
        double y_ms2 = y_raw_ms2;

        switch (motion_rotation_deg_.load(std::memory_order_relaxed)) {
        case 90:
            x_ms2 = y_raw_ms2;
            y_ms2 = -x_raw_ms2;
            break;

        case 180:
            x_ms2 = -x_raw_ms2;
            y_ms2 = -y_raw_ms2;
            break;

        case 270:
            x_ms2 = -y_raw_ms2;
            y_ms2 = x_raw_ms2;
            break;

        default:
            break;
        }

        // Collect completed batches under the list lock, but invoke the guest
        // callbacks outside it: they take the kernel lock, and SVC handlers
        // holding the kernel lock take list_lock_ through listen/cancel — the
        // reverse order would deadlock. The pump guard (held by our caller)
        // still serialises us against driver teardown.
        struct ready_batch {
            sensor_data_callback callback_;
            std::vector<std::uint8_t> data_;
            std::size_t packet_count_;
        };
        std::vector<ready_batch> ready;

        {
            const std::lock_guard<std::mutex> hold(list_lock_);

            if (!listening_list_.empty()) {
                common::double_linked_queue_element *first = listening_list_.first();
                common::double_linked_queue_element *end = listening_list_.end();

                do {
                    sensor_ios *sensor_obj = E_LOFF(first, sensor_ios, listening_link_);
                    sensor_obj->push_sample(x_ms2, y_ms2, z_ms2);

                    ready_batch batch;
                    if (sensor_obj->take_ready_batch(batch.callback_, batch.data_, batch.packet_count_)) {
                        ready.push_back(std::move(batch));
                    }

                    first = first->next;
                } while (first != end);
            }
        }

        for (ready_batch &batch : ready) {
            batch.callback_(batch.data_, batch.packet_count_);
        }
    }

    void sensor_driver_ios::track_active_listener(common::double_linked_queue_element *link) {
        const std::lock_guard<std::mutex> hold(list_lock_);
        listening_list_.push(link);
        refresh_pump_locked();
    }

    void sensor_driver_ios::untrack_active_listener(common::double_linked_queue_element *link) {
        const std::lock_guard<std::mutex> hold(list_lock_);
        link->deque();
        refresh_pump_locked();
    }

    std::vector<sensor_info> sensor_driver_ios::queries_active_sensor(const sensor_info &search_info) {
        std::vector<sensor_info> results;

        if (!accelerometer_available()) {
            return results;
        }

        sensor_info info;
        info.type_ = SENSOR_TYPE_ACCELEROMETER;
        info.quantity_ = SENSOR_DATA_QUANTITY_ACCELERATION;
        info.data_type_ = SENSOR_DATA_TYPE_ACCELOREMETER_AXIS;
        info.item_size_ = sizeof(sensor_accelerometer_axis_data);
        info.name_ = "iOS Accelerometer";
        info.vendor_ = "Apple";
        info.id_ = ACCELEROMETER_SENSOR_ID;

        // Unset (zero) fields in the search pattern match everything, the same
        // filtering the null backend applies.
        if (search_info.type_ && (search_info.type_ != info.type_)) {
            return results;
        }

        if (search_info.data_type_ && (search_info.data_type_ != info.data_type_)) {
            return results;
        }

        if (search_info.quantity_ && (search_info.quantity_ != info.quantity_)) {
            return results;
        }

        results.push_back(info);
        return results;
    }

    std::unique_ptr<sensor> sensor_driver_ios::new_sensor_controller(const std::uint32_t id) {
        if ((id != ACCELEROMETER_SENSOR_ID) || !accelerometer_available()) {
            return nullptr;
        }

        return std::make_unique<sensor_ios>(this);
    }

    bool sensor_driver_ios::pause() {
        {
            const std::lock_guard<std::mutex> hold(list_lock_);
            if (paused_) {
                return false;
            }

            paused_ = true;
            refresh_pump_locked();
        }

        // Barrier: wait out a handler that already left the operation queue,
        // so callers can tear down guest state (system reboot, backgrounding)
        // knowing no sensor callback is still in flight.
        const std::lock_guard<std::mutex> barrier(pump_->guard_->lock_);
        return true;
    }

    bool sensor_driver_ios::resume() {
        const std::lock_guard<std::mutex> hold(list_lock_);
        if (!paused_) {
            return false;
        }

        paused_ = false;
        refresh_pump_locked();
        return true;
    }
}
