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

#include <drivers/sensor/sensor.h>
#include "backend/null/sensor_null.h"

namespace eka2l1::drivers {
    void sensor_property_data::set_int(const std::int32_t int_value, const std::int32_t item_index) {
        data_type_ = DATA_TYPE_INT;
        item_index_ = item_index;
        array_index_ = SENSOR_PROPERTY_SINGLE;
        int_value_ = int_value;
    }

    void sensor_property_data::set_double(const double value, const std::int32_t item_index) {
        data_type_ = DATA_TYPE_DOUBLE;
        item_index_ = item_index;
        array_index_ = SENSOR_PROPERTY_SINGLE;
        float_value_ = value;
    }

    void sensor_property_data::set_buffer(const std::string &buffer, const std::int32_t item_index) {
        data_type_ = DATA_TYPE_BUFFER;
        item_index_ = item_index;
        array_index_ = SENSOR_PROPERTY_SINGLE;
        buffer_data_ = buffer;
    }

    void sensor_property_data::set_as_array_status(data_type type, const std::int32_t max_num, const std::int32_t active_element) {
        data_type_ = type;
        array_index_ = SENSOR_PROPERTY_ARRAY;
        item_index_ = -1;

        min_int_value_ = 0;
        max_int_value_ = max_num - 1;

        int_value_ = active_element;
    }

    std::unique_ptr<sensor_driver> sensor_driver::instantiate() {
        return std::make_unique<sensor_driver_null>();
    }
}