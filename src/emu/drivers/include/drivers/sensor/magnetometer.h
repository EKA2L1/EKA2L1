/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <drivers/sensor/sensor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace eka2l1::drivers {
    // Modelled on a 13-bit AK8975-class part: 0.3 uT per count.
    constexpr std::int32_t MAGNETOMETER_SCALE_RANGE_MAX = 4095;
    constexpr std::int32_t MAGNETOMETER_SCALE_RANGE_MIN = -4096;
    constexpr double MAGNETOMETER_MICROTESLA_PER_COUNT = 0.3;
    constexpr std::int32_t MAGNETOMETER_SCALE_MICRO = -6;
    constexpr std::int32_t MAGNETOMETER_CALIBRATION_LEVEL_HIGH = 3;

#pragma pack(push, 1)
    // TSensrvMagnetometerAxisData.
    struct sensor_magnetometer_axis_data {
        std::uint64_t timestamp_;
        std::int32_t axis_x_raw_;
        std::int32_t axis_y_raw_;
        std::int32_t axis_z_raw_;
        std::int32_t axis_x_calibrated_;
        std::int32_t axis_y_calibrated_;
        std::int32_t axis_z_calibrated_;
    };
#pragma pack(pop)

    static_assert(sizeof(sensor_magnetometer_axis_data) == 32);

    inline std::int32_t magnetometer_counts_from_microtesla(const double microtesla) {
        return static_cast<std::int32_t>(std::clamp<double>(std::round(microtesla / MAGNETOMETER_MICROTESLA_PER_COUNT),
            MAGNETOMETER_SCALE_RANGE_MIN, MAGNETOMETER_SCALE_RANGE_MAX));
    }

    // Field vectors are in uT, in the Symbian device frame (X right, Y up, Z out of the screen).
    inline sensor_magnetometer_axis_data magnetometer_from_microtesla(const std::uint64_t timestamp,
        const double *raw, const double *calibrated) {
        return { timestamp, magnetometer_counts_from_microtesla(raw[0]), magnetometer_counts_from_microtesla(raw[1]),
            magnetometer_counts_from_microtesla(raw[2]), magnetometer_counts_from_microtesla(calibrated[0]),
            magnetometer_counts_from_microtesla(calibrated[1]), magnetometer_counts_from_microtesla(calibrated[2]) };
    }

    // Every magnetometer channel property except the sample rate, which each backend owns.
    inline bool get_magnetometer_property(const sensor_property prop, sensor_property_data &data) {
        switch (prop) {
        case SENSOR_PROPERTY_DATA_FORMAT:
            data.set_int(SENSOR_DATA_FORMAT_SCALED);
            return true;

        // TSensrvProperty::GetMinValue(TInt&) panics on a non-int property.
        case SENSOR_PROPERTY_SCALED_RANGE:
            data.set_int(MAGNETOMETER_SCALE_RANGE_MAX);
            data.set_int_range(MAGNETOMETER_SCALE_RANGE_MIN, MAGNETOMETER_SCALE_RANGE_MAX);
            return true;

        case SENSOR_PROPERTY_MEASURE_RANGE: {
            const double max_value = MAGNETOMETER_SCALE_RANGE_MAX * MAGNETOMETER_MICROTESLA_PER_COUNT;
            data.set_double(max_value);
            data.set_double_range(MAGNETOMETER_SCALE_RANGE_MIN * MAGNETOMETER_MICROTESLA_PER_COUNT, max_value);
            return true;
        }

        case SENSOR_PROPERTY_CHANNEL_UNIT:
            data.set_int(SENSOR_UNIT_TELSA);
            return true;

        case SENSOR_PROPERTY_SCALE:
            data.set_int(MAGNETOMETER_SCALE_MICRO);
            return true;

        case SENSOR_PROPERTY_AVAILABILITY:
        case SENSOR_PROPERTY_MAGNETOMETER_AUTO_CALIBRATION_ACTIVE:
        case SENSOR_PROPERTY_MAGNETOMETER_AUTO_CALIBRATION_SUPPORTED:
            data.set_int(1);
            data.set_int_range(0, 1);
            return true;

        // The host OS calibrates the field it reports.
        case SENSOR_PROPERTY_MAGNETOMETER_CALIBRATION_LEVEL:
            data.set_int(MAGNETOMETER_CALIBRATION_LEVEL_HIGH);
            data.set_int_range(0, MAGNETOMETER_CALIBRATION_LEVEL_HIGH);
            return true;

        default:
            return false;
        }
    }

    // The host always calibrates, so only switching auto-calibration on is accepted. That reports
    // the calibration level to property listeners, as a sensor finishing its calibration would.
    inline bool set_magnetometer_property(const sensor_property_data &data,
        std::optional<sensor_property_data> &changed) {
        if ((data.property_id_ != SENSOR_PROPERTY_MAGNETOMETER_AUTO_CALIBRATION_ACTIVE) || (data.data_type_ != sensor_property_data::DATA_TYPE_INT)
            || (data.int_value_ != 1)) {
            return false;
        }

        sensor_property_data level;
        level.property_id_ = SENSOR_PROPERTY_MAGNETOMETER_CALIBRATION_LEVEL;
        get_magnetometer_property(level.property_id_, level);
        changed = level;

        return true;
    }
}
