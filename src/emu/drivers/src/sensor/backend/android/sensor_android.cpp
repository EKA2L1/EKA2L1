/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include "sensor_android.h"
#include <common/log.h>
#include <common/time.h>
#include <common/android/jniutils.h>
#include <thread>

namespace eka2l1::drivers {
    static constexpr int SENSOR_LOOPER_ID = 3;

    // These values are taken from Nokia 5800 Xpress Music
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

    sensor_android::sensor_android(sensor_driver_android *driver, ASensorRef const asensor)
        : driver_(driver)
        , asensor_(asensor)
        , event_queue_(nullptr)
        , listening_(false)
        , queried_(0)
        , data_callback_(nullptr)
        , packet_size_(0)
        , active_accel_measure_range_(0)
        , active_sampling_rate_(1)
        , sensor_type_(0) {
        event_queue_ = ASensorManager_createEventQueue(ASensorManager_getInstance(), common::jni::main_looper,
                                        SENSOR_LOOPER_ID, [](int fd, int events, void *data) {
            sensor_android *sensor = reinterpret_cast<sensor_android*>(data);
            sensor->handle_event_arrival();

            return 1;
        }, this);

        if (!event_queue_) {
            LOG_ERROR(SERVICE_SENSOR, "Unable to create event queue to receive sensor event!");
            return;
        }

        sensor_type_ = ASensor_getType(asensor);

        switch (sensor_type_) {
            case ASENSOR_TYPE_ACCELEROMETER:
                packet_size_ = sizeof(sensor_accelerometer_axis_data);
                break;

            default:
                packet_size_ = 100;
                break;
        }
    }

    sensor_android::~sensor_android() {
        cancel_data_listening();

        if (event_queue_) {
            ASensorManager_destroyEventQueue(ASensorManager_getInstance(), event_queue_);
        }
    }

    bool sensor_android::get_property(const sensor_property prop, const std::int32_t item_index,
                      const std::int32_t array_index, sensor_property_data &data) {
        // Handle global property
        bool global_handled = true;

        switch (prop) {
            case SENSOR_PROPERTY_SAMPLE_RATE:
                if (array_index == -2) {
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

            default:
                global_handled = false;
                break;
        }

        if (global_handled) {
            return true;
        }

        if (sensor_type_ == ASENSOR_TYPE_ACCELEROMETER) {
            data.property_id_ = prop;

            switch (prop) {
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
                    if (array_index == -2) {
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
        } else {
            LOG_ERROR(SERVICE_SENSOR, "Get property unimplemented for Android sensor type {}!", sensor_type_);
            return false;
        }

        return true;
    }

    bool sensor_android::set_property(const sensor_property_data &data) {
        bool global_handled = true;
        switch (data.property_id_) {
            case SENSOR_PROPERTY_SAMPLE_RATE:
                if (data.array_index_ == -2) {
                    if ((data.int_value_ >= SAMPLING_RATE_MAX_OPTION) || (data.int_value_ < 0)) {
                        LOG_ERROR(SERVICE_SENSOR, "Trying to set out-of-bound sample rate!");
                        return false;
                    }

                    active_sampling_rate_ = static_cast<std::uint32_t>(data.int_value_);
                    ASensorEventQueue_setEventRate(event_queue_, asensor_, static_cast<std::int32_t>(
                            common::microsecs_per_sec / SAMPLING_RATE_AVAILABLE[active_sampling_rate_]));
                } else {
                    LOG_ERROR(SERVICE_SENSOR, "Trying to set read-only sample rate property! Only current sample rate index can be set!");
                    return false;
                }

            default:
                global_handled = false;
                break;
        }

        if (global_handled) {
            return true;
        }

        if (sensor_type_ == ASENSOR_TYPE_ACCELEROMETER) {
            switch (data.property_id_) {
                case SENSOR_PROPERTY_MEASURE_RANGE:
                    if (data.array_index_ == -2) {
                        if ((data.int_value_ >= ACCELEROMETER_MEASURE_RANGE_MAX_OPTION) || (data.int_value_ < 0)) {
                            LOG_ERROR(SERVICE_SENSOR, "Trying to set out-of-bound measure range!");
                            return false;
                        }

                        active_accel_measure_range_ = static_cast<std::uint32_t>(data.int_value_);
                    } else {
                        LOG_ERROR(SERVICE_SENSOR, "Trying to set read-only measure range property! Only current measure range index can be set!");
                        return false;
                    }

                    break;

                default:
                    LOG_TRACE(SERVICE_SENSOR, "Unhandled setting accelerometer sensor property {}", static_cast<int>(data.property_id_));
                    break;
            }
        } else {
            LOG_ERROR(SERVICE_SENSOR, "Set property unimplemented for Android sensor type {}!", sensor_type_);
            return false;
        }

        return false;
    }

    void sensor_android::translate_events_to_buffer(const std::size_t count) {
        for (std::size_t i = 0; i < count; i++) {
            translate_single_event_and_push(events_[i]);
        }
    }

    void sensor_android::translate_single_event_and_push(const ASensorEvent &event) {
        // NOTE: The time given by Android to native is elapsed time in nanosecs since bootup
        // But there's no utilities widely available to convert them to EPOCH. So we gonna use the current time here
        // Hope it's fine :D
        switch (event.type) {
            case ASENSOR_TYPE_ACCELEROMETER: {
                sensor_accelerometer_axis_data axis_data;

                static constexpr float alpha = 0.8f;
                const float measure_range = ACCELEROMETER_MEASURE_RANGE_AVAILABLE[active_accel_measure_range_];

                axis_data.timestamp_ = common::get_current_utc_time_in_microseconds_since_0ad();

                axis_data.axis_x_ = static_cast<std::int32_t>(static_cast<float>(event.acceleration.x) * ACCELEROMETER_SCALE_RANGE_MAX / measure_range);
                axis_data.axis_y_ = static_cast<std::int32_t>(static_cast<float>(event.acceleration.y) * ACCELEROMETER_SCALE_RANGE_MAX / measure_range);
                axis_data.axis_z_ = static_cast<std::int32_t>(static_cast<float>(event.acceleration.z) * ACCELEROMETER_SCALE_RANGE_MAX / measure_range);

                events_translated_.insert(events_translated_.end(), reinterpret_cast<std::uint8_t*>(&axis_data),
                                          reinterpret_cast<std::uint8_t*>(&axis_data + 1));

                break;
            }

            default:
                LOG_WARN(SERVICE_SENSOR, "Unknown event type {}", event.type);
                break;
        }
    }

    void sensor_android::handle_event_arrival() {
        const std::lock_guard<std::mutex> guard(lock_);
        if (!data_callback_) {
            return;
        }

        static constexpr std::uint64_t MIN_REPORT_TIME_US_QUOTA = 60;
        const std::uint64_t now = common::get_current_utc_time_in_microseconds_since_epoch();

        ssize_t gotten = ASensorEventQueue_getEvents(event_queue_, events_.data(), events_.size() - queried_);
        if (gotten < 0) {
            LOG_ERROR(SERVICE_SENSOR, "Get events returned {}!", gotten);
            return;
        } else {
            queried_ += gotten;
        }

        translate_events_to_buffer(static_cast<std::size_t>(gotten));

        if (queried_ == events_.size()) {
            sensor_data_callback callback_copy = data_callback_;

            data_callback_ = nullptr;
            callback_copy(events_translated_, queried_);
        }
    }

    void sensor_android::receive_data(sensor_data_callback callback) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (data_callback_) {
            return;
        }

        queried_ = 0;
        data_callback_ = callback;
        events_translated_.clear();
    }

    bool sensor_android::enable_sensor_with_params() {
        // TODO: Delay is currently ignored. Need register sensor I think!
        // Register sensor needs SDK ver 26. We target 21
        int result = ASensorEventQueue_enableSensor(event_queue_, asensor_);
        if (result < 0) {
            LOG_ERROR(SERVICE_SENSOR, "Register sensor event listening failed with result {}", result);
            return false;
        }

        const std::int32_t period_event = static_cast<std::int32_t>(common::microsecs_per_sec /
                SAMPLING_RATE_AVAILABLE[active_sampling_rate_]);

        result = ASensorEventQueue_setEventRate(event_queue_, asensor_, period_event);

        if (result < 0) {
            LOG_WARN(SERVICE_SENSOR, "Setting sensor event rate failed with result {}. us/event={}",
                     result, period_event);
        }

        return true;
    }

    bool sensor_android::listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) {
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

        if (!enable_sensor_with_params()) {
            return false;
        }

        events_.resize(desired_buffering_count);
        driver_->track_active_listener(&listening_link_);

        listening_ = true;

        return true;
    }

    bool sensor_android::cancel_data_listening() {
        if (!listening_) {
            return false;
        }

        // Disable the sensor for this queue
        int result = ASensorEventQueue_disableSensor(event_queue_, asensor_);
        if (result < 0) {
            LOG_ERROR(SERVICE_SENSOR, "Failed to disable event listening with result {}", result);
            return false;
        }

        listening_ = false;
        listening_link_.deque();

        return true;
    }

    void sensor_android::pause_data_listening() {
        if (!listening_) {
            return;
        }
        int result = ASensorEventQueue_disableSensor(event_queue_, asensor_);
        if (result < 0) {
            LOG_ERROR(SERVICE_SENSOR, "Failed to pause event listening with result {}", result);
        }
    }

    void sensor_android::resume_data_listening() {
        if (!listening_) {
            return;
        }

        enable_sensor_with_params();
    }

    std::vector<sensor_property_data> sensor_android::get_all_properties(const sensor_property *prop_value) {
        LOG_ERROR(SERVICE_SENSOR, "Set all properties unimplemented!");
        return std::vector<sensor_property_data>{};
    };

    sensor_driver_android::sensor_driver_android()
        : all_sensors_(nullptr)
        , sensor_count_(0)
        , paused_(false) {
    }

    sensor_driver_android::~sensor_driver_android() {
        while (!listening_list_.empty()) {
            sensor_android *sensor = E_LOFF(listening_list_.first()->deque(), sensor_android, listening_link_);
            sensor->cancel_data_listening();
        }
    }

    std::vector<sensor_info> sensor_driver_android::queries_active_sensor(const sensor_info &search_info) {
        std::vector<sensor_info> results;

        if (!all_sensors_) {
            ASensorManager *sensor_manager = ASensorManager_getInstance();
            sensor_count_ = ASensorManager_getSensorList(sensor_manager, &all_sensors_);

            if (sensor_count_ < 0) {
                sensor_count_ = 0;

                LOG_ERROR(DRIVER_SENSOR, "Unable to get sensor device list!");
                return results;
            }
        }

        sensor_info info;

        for (int i = 0; i < sensor_count_; i++) {
            int type = ASensor_getType(all_sensors_[i]);

            // Add and handle types that we currently supported here!
            switch (type) {
                case ASENSOR_TYPE_ACCELEROMETER:
                    info.type_ = SENSOR_TYPE_ACCELEROMETER;
                    info.quantity_ = SENSOR_DATA_QUANTITY_ACCELERATION;
                    info.data_type_ = SENSOR_DATA_TYPE_ACCELOREMETER_AXIS;
                    info.item_size_ = sizeof(sensor_accelerometer_axis_data);
                    break;

                default:
                    continue;
            }

            if (search_info.type_ != info.type_) {
                continue;
            }

            info.name_ = ASensor_getName(all_sensors_[i]);
            info.vendor_ = ASensor_getVendor(all_sensors_[i]);
            info.id_ = static_cast<std::uint32_t>(i + 1);

            results.push_back(info);
        }

        return results;
    }

    std::unique_ptr<sensor> sensor_driver_android::new_sensor_controller(const std::uint32_t id) {
        if ((id == 0) || (id > static_cast<std::uint32_t>(sensor_count_))) {
            return nullptr;
        }

        return std::make_unique<sensor_android>(this, all_sensors_[id - 1]);
    }

    void sensor_driver_android::track_active_listener(common::double_linked_queue_element *link) {
        listening_list_.push(link);
    }

    bool sensor_driver_android::pause() {
        if (paused_) {
            return false;
        }

        if (!listening_list_.empty()) {
            common::double_linked_queue_element *first = listening_list_.first();
            common::double_linked_queue_element *end = listening_list_.end();

            do {
                sensor_android *sensor_obj = E_LOFF(first, sensor_android, listening_link_);
                sensor_obj->pause_data_listening();

                first = first->next;
            } while (first != end);
        }

        paused_ = true;
        return true;
    }

    bool sensor_driver_android::resume() {
        if (!paused_) {
            return false;
        }

        if (!listening_list_.empty()) {
            common::double_linked_queue_element *first = listening_list_.first();
            common::double_linked_queue_element *end = listening_list_.end();

            do {
                sensor_android *sensor_obj = E_LOFF(first, sensor_android, listening_link_);
                sensor_obj->resume_data_listening();

                first = first->next;
            } while (first != end);
        }

        paused_ = false;
        return true;
    }
}