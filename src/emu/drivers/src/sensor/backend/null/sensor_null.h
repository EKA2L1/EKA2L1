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

#include <drivers/sensor/sensor.h>
#include <vector>

namespace eka2l1::drivers {
    class sensor_null_base: public sensor {
    private:
        sensor_info info_;

    protected:
        std::vector<sensor_property_data> properties_datas_;

    public:
        explicit sensor_null_base(const sensor_info &info);

        bool get_property(const sensor_property prop, const std::int32_t item_index,
            const std::int32_t array_index, sensor_property_data &data) override;
        bool set_property(const sensor_property_data &data) override;

        bool listen_for_data(std::size_t desired_buffering_count, std::size_t max_buffering_count, std::size_t delay_us) override;
        bool cancel_data_listening() override;
        void receive_data(sensor_data_callback callback) override;

        std::vector<sensor_property_data> get_all_properties(const sensor_property *prop_value = nullptr) override;

        std::uint32_t data_packet_size() const override {
            return 100;
        }
    };

    class sensor_accelerometer_null: public sensor_null_base {
    public:
        explicit sensor_accelerometer_null(const sensor_info &info);
    };

    class sensor_driver_null: public sensor_driver {
    private:
        std::vector<sensor_info> stubbed_infos_;

    public:
        explicit sensor_driver_null();

        std::vector<sensor_info> queries_active_sensor(const sensor_info &search_info) override;
        std::unique_ptr<sensor> new_sensor_controller(const std::uint32_t id) override;

        bool pause() override { return true; }
        bool resume() override { return true; }
    };
}