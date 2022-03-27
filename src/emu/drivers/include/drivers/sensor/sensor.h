/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace eka2l1::drivers {
    // NOTE: This matchs Symbian's Sensor API to reduce less translation.
    enum sensor_type {
        SENSOR_TYPE_UNDEFINED = 0,
        SENSOR_TYPE_ACCELEROMETER = 0x1020507E,
        SENSOR_TYPE_MAGNEGTOMETER = 0x2000BEE0,
        SENSOR_TYPE_MAGNECTIC_NORTH_ANGLE = 0x2000BEDF,
        SENSOR_TYPE_AMBIENT_LIGHT = 0x2000BF16,
        SENSOR_TYPE_PROXIMOTY = 0x2000E585,
        SENSOR_TYPE_ORIENTATION = 0x10205088,
        SENSOR_TYPE_ROTATION = 0x10205089,
        SENSOR_TYPE_ALL = 0xFFFFFFFF
    };

    enum sensor_data_type {
        SENSOR_DATA_TYPE_UNDEFINED = 0,
        SENSOR_DATA_TYPE_ACCELOREMETER_AXIS = 0x1020507E,
        SENSOR_DATA_TYPE_AMBIENT_LIGHT = 0x2000BF16,
        SENSOR_DATA_TYPE_MAGNECTIC_NORTH_ANGLE = 0x2000BEDF,
        SENSOR_DATA_TYPE_MAGNEGTOMETER_AXIS = 0x2000BEE0,
        SENSOR_DATA_TYPE_ORIENTATION = 0x10205088,
        SENSOR_DATA_TYPE_ROTATION = 0x10205089,
        SENSOR_DATA_TYPE_PROXIMOTY = 0x2000E585
    };

    enum sensor_data_quantity {
        SENSOR_DATA_QUANTITY_NOT_USED = -1,
        SENSOR_DATA_QUANTITY_NOT_DEFINED = 0,
        SENSOR_DATA_QUANTITY_ACCELERATION = 10,
        SENSOR_DATA_QUANTITY_TAPPING = 11,
        SENSOR_DATA_QUANTITY_ORIENTATION = 12,
        SENSOR_DATA_QUANTITY_ROTATION = 13,
        SENSOR_DATA_QUANTITY_MAGNEGTIC = 14,
        SENSOR_DATA_QUANTITY_ANGLE = 15,
        SENSOR_DATA_QUANTITY_PROXIMOTY = 16
    };

    enum sensor_connection_type {
        SENSOR_CONNECT_NOT_DEFINED = 0,
        SENSOR_CONNECT_TYPE_EMBEDDED = 1,
        SENSOR_CONNECT_TYPE_WIRED = 2,
        SENSOR_CONNECT_TYPE_WIRELESS = 3
    };

    enum sensor_unit {
        SENSOR_UNIT_NONE = 0,
        SENSOR_UNIT_MS_PER_S2 = 10,
        SENSOR_UNIT_GRAVITY = 11,
        SENSOR_UNIT_TELSA = 12 
    };

    enum sensor_data_format {
        SENSOR_DATA_FORMAT_UNDEFINED = 0,
        SENSOR_DATA_FORMAT_ABSOLUTE = 1,
        SENSOR_DATA_FORMAT_SCALED = 2
    };

    // NOTE: This matchs Symbian's Sensor API to reduce less translation.
    enum sensor_property {
        SENSOR_PROPERTY_SAMPLE_RATE = 0x2,
        SENSOR_PROPERTY_POWER_ON_OFF = 0x3,
        SENSOR_PROPERTY_AVAILABILITY = 0x4,
        SENSOR_PROPERTY_MEASURE_RANGE = 0x5,
        SENSOR_PROPERTY_DATA_FORMAT = 0x6,
        SENSOR_PROPERTY_SCALED_RANGE = 0x7,
        SENSOR_PROPERTY_ACCURACY = 0x8,
        SENSOR_PROPERTY_SCALE = 0x9,
        SENSOR_PROPERTY_CHANNEL_UNIT = 0x10,
        SENSOR_PROPERTY_MODEL_NAME = 0x11,
        SENSOR_PROPERTY_CONNECTION_TYPE = 0x12,
        SENSOR_PROPERTY_DESCRIPTION = 0x13,
        SENSOR_PROPERTY_ACCELEROMETER_AXIS_ACTIVE = 0x1001
    };

    struct sensor_info {
        sensor_type type_ = SENSOR_TYPE_UNDEFINED;
        sensor_data_type data_type_ = SENSOR_DATA_TYPE_UNDEFINED;
        sensor_data_quantity quantity_ = SENSOR_DATA_QUANTITY_NOT_DEFINED;

        std::string name_;
        std::string vendor_;
        std::string location_;

        std::uint32_t id_ = 0;
        std::uint32_t item_size_ = 0;
    };

    enum sensor_property_array_index {
        SENSOR_PROPERTY_SINGLE = -1,
        SENSOR_PROPERTY_ARRAY = -2
    };

    struct sensor_property_data {
        sensor_property property_id_;

        enum data_type {
            DATA_TYPE_INT,
            DATA_TYPE_DOUBLE,
            DATA_TYPE_BUFFER
        } data_type_;

        std::int32_t array_index_;
        std::int32_t item_index_;

        union {
            struct {
                std::int32_t int_value_;
                std::int32_t min_int_value_;
                std::int32_t max_int_value_;
            };

            struct {
                double float_value_;
                double min_float_value_;
                double max_float_value_;
            };
        };

        std::string buffer_data_;

        // Use these functions to not forgot setting buffer type.
        // Many times we do :(
        void set_int(const std::int32_t int_value, const std::int32_t item_index = -1);
        void set_double(const double float_value, const std::int32_t item_index = -1);
        void set_int_range(const std::int32_t min, const std::int32_t max);
        void set_double_range(const double min, const double max);
        void set_buffer(const std::string &buffer, const std::int32_t item_index = -1);

        void set_as_array_status(data_type type, const std::int32_t max_num, const std::int32_t active_element);
    };

#pragma pack(push, 1)
    struct sensor_accelerometer_axis_data {
        std::uint64_t timestamp_;
        std::int32_t axis_x_;
        std::int32_t axis_y_;
        std::int32_t axis_z_;
        std::int32_t pad_ = 0;
    };
#pragma pack(pop)

    static_assert(sizeof(sensor_accelerometer_axis_data) == 24);

    using sensor_data_callback = std::function<void(std::vector<std::uint8_t> &, std::size_t)>;

    // TODO: Property listening
    class sensor {
    public:
        virtual ~sensor() = default;
        virtual bool get_property(const sensor_property prop, const std::int32_t item_index,
            const std::int32_t array_index, sensor_property_data &data) = 0;
        virtual bool set_property(const sensor_property_data &data) = 0;
        virtual std::vector<sensor_property_data> get_all_properties(const sensor_property *prop_value = nullptr) = 0;

        /**
         * @brief Listen for sensor data.
         * 
         * @param desired_buffering_count   Number of packets desired to received on the callback.
         * @param max_buffering_count       Maximum number of packets to received on the callback.
         * @param delay_us                  Specify the time after all packets are prepared, to call the callback.
         * 
         * @returns True on success.
         */
        virtual bool listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) = 0;
        virtual bool cancel_data_listening() = 0;

        /**
         * Asynchronously receive sensor data.
         *
         * Listen must be called first!
         *
         * @param callback Function that called when the amount of desired packets have arrived.
         */
        virtual void receive_data(sensor_data_callback callback) = 0;

        virtual std::uint32_t data_packet_size() const = 0;
    };

    class sensor_driver {
    public:
        virtual std::vector<sensor_info> queries_active_sensor(const sensor_info &search_info) = 0;
        virtual std::unique_ptr<sensor> new_sensor_controller(const std::uint32_t id) = 0;

        /**
         * Pause all sensor listeners. Use to save battery when inactive.
         * @return True on success.
         */
        virtual bool pause() = 0;

        /**
         * Resume all active sensor listeners. Use when app is back from background.
         * @return True on success.
         */
        virtual bool resume() = 0;

        static std::unique_ptr<sensor_driver> instantiate();
    };
}