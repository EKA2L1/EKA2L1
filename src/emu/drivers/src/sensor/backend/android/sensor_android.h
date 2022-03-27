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

#pragma once

#include <common/linked.h>
#include <drivers/sensor/sensor.h>
#include <android/sensor.h>
#include <mutex>
#include <vector>

namespace eka2l1::drivers {
    class sensor_driver_android;

    class sensor_android: public sensor {
    private:
        friend class sensor_driver_android;

        sensor_driver_android *driver_;
        ASensorRef const asensor_;
        ASensorEventQueue *event_queue_;
        bool listening_;

        std::vector<ASensorEvent> events_;
        std::vector<std::uint8_t> events_translated_;
        std::size_t queried_;

        sensor_data_callback data_callback_;
        std::uint32_t packet_size_;
        std::uint32_t active_accel_measure_range_;
        std::uint32_t active_sampling_rate_;
        int sensor_type_;
        std::mutex lock_;

        common::double_linked_queue_element listening_link_;

        void translate_events_to_buffer(const std::size_t count);
        void translate_single_event_and_push(const ASensorEvent &event);
        bool enable_sensor_with_params();

    public:
        explicit sensor_android(sensor_driver_android *driver, ASensorRef const asensor);
        ~sensor_android() override;

        bool get_property(const sensor_property prop, const std::int32_t item_index,
                          const std::int32_t array_index, sensor_property_data &data) override;
        bool set_property(const sensor_property_data &data) override;

        bool listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) override;
        bool cancel_data_listening() override;
        void pause_data_listening();
        void resume_data_listening();

        void receive_data(sensor_data_callback callback) override;

        std::vector<sensor_property_data> get_all_properties(const sensor_property *prop_value = nullptr) override;

        void handle_event_arrival();

        std::uint32_t data_packet_size() const override {
            return packet_size_;
        }
    };

    class sensor_driver_android: public sensor_driver {
    private:
        ASensorList all_sensors_;
        int sensor_count_;

        common::roundabout listening_list_;
        bool paused_;

    public:
        explicit sensor_driver_android();
        ~sensor_driver_android();

        std::vector<sensor_info> queries_active_sensor(const sensor_info &search_info) override;
        std::unique_ptr<sensor> new_sensor_controller(const std::uint32_t id) override;

        bool pause() override;
        bool resume() override;

        void track_active_listener(common::double_linked_queue_element *link);
    };
}