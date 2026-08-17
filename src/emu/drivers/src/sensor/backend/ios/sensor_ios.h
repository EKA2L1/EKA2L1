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

#pragma once

#include <common/linked.h>
#include <drivers/sensor/sensor.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace eka2l1::drivers {
    class sensor_driver_ios;

    // Owns the CoreMotion objects (ObjC types live only in the .mm so this
    // header stays includable from plain C++, e.g. the driver factory).
    struct core_motion_pump;

    class sensor_ios : public sensor {
    private:
        friend class sensor_driver_ios;

        sensor_driver_ios *driver_;
        bool listening_;

        std::vector<std::uint8_t> events_translated_;
        std::size_t packets_wanted_;
        std::size_t packets_buffered_;

        sensor_data_callback data_callback_;
        std::uint32_t active_accel_measure_range_;
        std::uint32_t active_sampling_rate_;
        std::mutex lock_;

        common::double_linked_queue_element listening_link_;

        // Called by the driver's CoreMotion pump for each accelerometer
        // sample, already converted to the Android/Symbian m/s^2 convention.
        void push_sample(const double x_ms2, const double y_ms2, const double z_ms2);

        // Move out the armed callback plus the buffered packets when enough
        // have accumulated, so the pump can invoke the callback lock-free.
        bool take_ready_batch(sensor_data_callback &callback_out, std::vector<std::uint8_t> &data_out,
            std::size_t &packet_count_out);

    public:
        explicit sensor_ios(sensor_driver_ios *driver);
        ~sensor_ios() override;

        bool get_property(const sensor_property prop, const std::int32_t item_index,
            const std::int32_t array_index, sensor_property_data &data) override;
        bool set_property(const sensor_property_data &data) override;

        bool listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) override;
        bool cancel_data_listening() override;

        void receive_data(sensor_data_callback callback) override;

        std::vector<sensor_property_data> get_all_properties(const sensor_property *prop_value = nullptr) override;

        std::uint32_t data_packet_size() const override {
            return sizeof(sensor_accelerometer_axis_data);
        }
    };

    class sensor_driver_ios : public sensor_driver {
    private:
        friend class sensor_ios;

        std::unique_ptr<core_motion_pump> pump_;

        common::roundabout listening_list_;
        std::mutex list_lock_;
        bool paused_;

        // CCW angle from the iPhone's natural orientation to the emulated
        // device's natural orientation (see sensor_driver::set_motion_rotation).
        // Written by the frontend's present path, read on the CoreMotion queue.
        std::atomic<int> motion_rotation_deg_;

        void track_active_listener(common::double_linked_queue_element *link);
        void untrack_active_listener(common::double_linked_queue_element *link);

        // Starts / stops / retunes CoreMotion accelerometer updates to match
        // the current listener set, pause state and requested sampling rate.
        // Callers must hold list_lock_.
        void refresh_pump_locked();

        // Locking variant, used by sensors when their sampling rate changes.
        void refresh_pump();

        // Pump handler: fan a sample out to every listening sensor.
        void dispatch_sample(const double x_ms2, const double y_ms2, const double z_ms2);

        std::uint32_t max_requested_sampling_rate_locked();

    public:
        explicit sensor_driver_ios();
        ~sensor_driver_ios() override;

        std::vector<sensor_info> queries_active_sensor(const sensor_info &search_info) override;
        std::unique_ptr<sensor> new_sensor_controller(const std::uint32_t id) override;

        bool pause() override;
        bool resume() override;
        void set_motion_rotation(const int degrees) override;

        bool accelerometer_available() const;
    };
}
