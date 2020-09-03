/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/types.h>

#include <mutex>
#include <string>
#include <vector>

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::manager {
    struct device {
        epocver ver;
        std::string firmware_code;
        std::string manufacturer;
        std::string model;
        std::vector<int> languages;
        std::uint32_t machine_uid;
        int default_language_code;
        std::uint16_t time_delay_us;
    };

    void get_recommended_stat_for_device(const epocver ver, std::uint16_t &time_delay_us);

    /*! \brief A manager for all installed devices on this emulator
    */
    class device_manager {
        std::vector<device> devices;
        device *current;
        config::state *conf;

    protected:
        void import_device_config_to_global(device *dvc);

    public:
        std::mutex lock;

        explicit device_manager(config::state *conf);
        ~device_manager();

        std::vector<device> &get_devices() {
            return devices;
        }

        std::size_t total() {
            return devices.size();
        }

        device *get_current() {
            return current;
        }

        void save_devices();
        void load_devices();

        bool set_current(const std::string &firmcode);
        bool set_current(const std::uint8_t idx);

        bool add_new_device(const std::string &firmcode, const std::string &model, const std::string &manufacturer, const epocver ver, const std::uint32_t machine_uid,
            const std::uint16_t time_delay_us);

        bool delete_device(const std::string &firmcode);

        void set_time_delay_in_us(const std::uint16_t delay_us);

        /*! \brief Get the device with the given firmware code.
         *
         * You should avoid method that involves comparing firmware code, since
         * the comparsion is case-sensitive. Use listing and index instead.
         * 
         * Not thread-safe.
         * 
         * \returns nullptr if the device can't be found
        */
        device *get(const std::string &firmcode);

        /*! \brief Get the device with the given index.
         *
         * Not thread-safe.
         * 
         * \returns nullptr if index out of range.
        */
        device *get(const std::uint8_t index);
    };
}