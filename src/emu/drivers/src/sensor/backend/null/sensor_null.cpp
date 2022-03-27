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

#include "sensor_null.h"
#include <common/log.h>

namespace eka2l1::drivers {
    sensor_null_base::sensor_null_base(const sensor_info &info)
        : info_(info) {
        // Description
        sensor_property_data description_data;
        description_data.property_id_ = SENSOR_PROPERTY_DESCRIPTION;
        description_data.set_buffer(info.vendor_ + " - " + info.name_);

        properties_datas_.push_back(description_data);

        // Sample rate
        sensor_property_data sample_rate;
        sample_rate.property_id_ = SENSOR_PROPERTY_SAMPLE_RATE;

        sample_rate.set_as_array_status(sensor_property_data::DATA_TYPE_INT, 1, 0);
        properties_datas_.push_back(sample_rate);

        sample_rate.set_int(10, 0);
        properties_datas_.push_back(sample_rate);

        // Connection
        sensor_property_data connection;
        connection.property_id_ = SENSOR_PROPERTY_CONNECTION_TYPE;
        connection.set_int(SENSOR_CONNECT_TYPE_EMBEDDED, 0);

        properties_datas_.push_back(connection);
    }

    bool sensor_null_base::get_property(const sensor_property prop, const std::int32_t item_index,
        const std::int32_t array_index, sensor_property_data &data) {
        data.property_id_ = prop;

        for (std::size_t i = 0; i < properties_datas_.size(); i++) {
            if ((properties_datas_[i].property_id_ == prop) && (properties_datas_[i].item_index_ == item_index)
                && (properties_datas_[i].array_index_ == array_index)) {
                data = properties_datas_[i];
                return true;
            }
        }

        // Success anyway, apps break or not is on you...
        LOG_WARN(DRIVER_SENSOR, "Unhandled sensor property to retrieve (id=0x{:X})", static_cast<int>(prop));
        return true;
    }

    std::vector<sensor_property_data> sensor_null_base::get_all_properties(const sensor_property *prop_value) {
        if (!prop_value) {
            return properties_datas_;
        }

        std::vector<sensor_property_data> results;
        for (std::size_t i = 0; i < properties_datas_.size(); i++) {
            if (properties_datas_[i].property_id_ == *prop_value) {
                results.push_back(properties_datas_[i]);
            }
        }

        return results;
    }

    bool sensor_null_base::set_property(const sensor_property_data &data) {
        return true;
    }

    bool sensor_null_base::listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) {
        // Return a fake handle.
        return true;
    }

    bool sensor_null_base::cancel_data_listening() {
        return true;
    }

    void sensor_null_base::receive_data(sensor_data_callback callback) {
    }

    sensor_accelerometer_null::sensor_accelerometer_null(const sensor_info &info)
        : sensor_null_base(info) {
        // Axis active
        sensor_property_data axis_active;
        axis_active.property_id_ = SENSOR_PROPERTY_ACCELEROMETER_AXIS_ACTIVE;
        axis_active.min_int_value_ = 0;
        axis_active.max_int_value_ = 1;

        axis_active.set_int(0, 1);
        properties_datas_.push_back(axis_active);

        axis_active.set_int(0, 2);        
        properties_datas_.push_back(axis_active);

        axis_active.set_int(0, 3);        
        properties_datas_.push_back(axis_active);

        // Measure range
        sensor_property_data measure_range;
        measure_range.property_id_ = SENSOR_PROPERTY_MEASURE_RANGE;
        measure_range.set_double(50.0f);
        measure_range.array_index_ = -2;            // Required by NGage 2.0?

        properties_datas_.push_back(measure_range);

        // Data unit
        sensor_property_data unit_prop;
        unit_prop.property_id_ = SENSOR_PROPERTY_CHANNEL_UNIT;
        unit_prop.set_int(SENSOR_UNIT_MS_PER_S2);

        properties_datas_.push_back(unit_prop);
    }

    sensor_driver_null::sensor_driver_null() {
        sensor_info info;

        // Shared variables
        info.location_ = "";

        // Accelerometer
        info.data_type_ = drivers::SENSOR_DATA_TYPE_ACCELOREMETER_AXIS;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_ACCELERATION;
        info.type_ = drivers::SENSOR_TYPE_ACCELEROMETER;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Accelerometer Sensor Stub";
        info.vendor_ = "Dog";

        stubbed_infos_.push_back(info);
        
        // Ambient light
        info.data_type_ = drivers::SENSOR_DATA_TYPE_AMBIENT_LIGHT;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_NOT_USED;
        info.type_ = drivers::SENSOR_TYPE_AMBIENT_LIGHT;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Ambient Light Sensor Stub";
        info.vendor_ = "Lamp";

        stubbed_infos_.push_back(info);
        
        // Angle between Z and pole
        info.data_type_ = drivers::SENSOR_DATA_TYPE_MAGNECTIC_NORTH_ANGLE;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_ANGLE;
        info.type_ = drivers::SENSOR_TYPE_MAGNECTIC_NORTH_ANGLE;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Magnectic North Angle Stub";
        info.vendor_ = "North Pole";

        stubbed_infos_.push_back(info);

        // Magnegtometer
        info.data_type_ = drivers::SENSOR_DATA_TYPE_MAGNEGTOMETER_AXIS;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_MAGNEGTIC;
        info.type_ = drivers::SENSOR_TYPE_MAGNEGTOMETER;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Magnegtometer Stub";
        info.vendor_ = "Magnet";

        stubbed_infos_.push_back(info);

        // Proximoty
        info.data_type_ = drivers::SENSOR_DATA_TYPE_PROXIMOTY;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_PROXIMOTY;
        info.type_ = drivers::SENSOR_TYPE_PROXIMOTY;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Proximoty Stub";
        info.vendor_ = "Local restaurant";

        stubbed_infos_.push_back(info);

        // Rotation
        info.data_type_ = drivers::SENSOR_DATA_TYPE_ROTATION;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_ROTATION;
        info.type_ = drivers::SENSOR_TYPE_ROTATION;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Rotation Stub";
        info.vendor_ = "Football";

        stubbed_infos_.push_back(info);

        // Orientation
        info.data_type_ = drivers::SENSOR_DATA_TYPE_ORIENTATION;
        info.quantity_ = drivers::SENSOR_DATA_QUANTITY_ORIENTATION;
        info.type_ = drivers::SENSOR_TYPE_ORIENTATION;
        info.id_ = static_cast<std::uint32_t>(stubbed_infos_.size() + 1);
        info.name_ = "EKA2L1 Orientation Stub";
        info.vendor_ = "British";

        stubbed_infos_.push_back(info);
    }

    std::vector<sensor_info> sensor_driver_null::queries_active_sensor(const sensor_info &search_info) {
        std::vector<sensor_info> results;
        for (std::size_t i = 0; i < stubbed_infos_.size(); i++) {
            if (search_info.data_type_ && (search_info.data_type_ != stubbed_infos_[i].data_type_)) {
                continue;
            }

            if (search_info.quantity_ && (search_info.quantity_ != stubbed_infos_[i].quantity_)) {
                continue;
            }

            if (search_info.type_ && (search_info.type_ != stubbed_infos_[i].type_)) {
                continue;
            }
            
            if (!search_info.name_.empty() && (search_info.name_ != stubbed_infos_[i].name_)) {
                continue;
            }

            if (!search_info.vendor_.empty() && (search_info.vendor_ != stubbed_infos_[i].vendor_)) {
                continue;
            }

            results.push_back(stubbed_infos_[i]);
        }

        return results;
    }

    std::unique_ptr<sensor> sensor_driver_null::new_sensor_controller(const std::uint32_t id) {
        if (id > stubbed_infos_.size()) {
            return nullptr;
        }

        sensor_info basis_info = stubbed_infos_[id - 1];
        if (basis_info.type_ == drivers::SENSOR_TYPE_ACCELEROMETER) {
            return std::make_unique<sensor_accelerometer_null>(basis_info);
        }

        return std::make_unique<sensor_accelerometer_null>(basis_info);
    }
}