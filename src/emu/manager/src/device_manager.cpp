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

#include <manager/device_manager.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>

namespace eka2l1::manager {
    void device_manager::load_devices() {
        YAML::Node devices_node {};

        try {
            devices_node = std::move(YAML::LoadFile("devices.yml"));
        } catch (YAML::Exception exception) {
            return;
        }

        for (auto &device_node: devices_node) {
            const std::string firmcode = device_node.first.as<std::string>();
            const std::string manufacturer = device_node.second["manufacturer"].as<std::string>();
            const std::string model = device_node.second["model"].as<std::string>();
            epocver ver = static_cast<epocver>(device_node.second["platver"].as<int>());

            add_new_device(firmcode, model, manufacturer, ver);
        }
    }

    void device_manager::save_devices() {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        for (const auto &device: devices) {
            emitter << YAML::Key << device.firmware_code;
            emitter << YAML::Value << YAML::BeginMap;

            emitter << YAML::Key << "platver" << YAML::Value << static_cast<const int>(device.ver);
            emitter << YAML::Key << "manufacturer" << YAML::Value << device.manufacturer;
            emitter << YAML::Key << "firmcode" << YAML::Value << device.firmware_code;
            emitter << YAML::Key << "model" << YAML::Value << device.model;

            emitter << YAML::EndMap;
        }

        emitter << YAML::EndMap;
        std::ofstream outdevicefile("devices.yml");

        outdevicefile << emitter.c_str();

        return;
    }

    device_manager::device_manager() {
        load_devices();
    }

    device_manager::~device_manager() {
        save_devices();
    }

    device *device_manager::get(const std::string &firmcode) {
        auto result = std::find_if(devices.begin(), devices.end(),
            [&](const device &dvc) { return dvc.firmware_code == firmcode; });

        if (result != devices.end()) {
            return &(*result);
        }

        return nullptr;
    }
    
    device *device_manager::get(const std::uint8_t index) {
        if (devices.size() <= index) {
            return nullptr;
        }

        return &(devices[index]);
    }
    
    bool device_manager::set_current(const std::string &firmcode) {
        const std::lock_guard<std::mutex> guard(lock);
        current = get(firmcode);

        return current;
    }
    
    bool device_manager::set_current(const std::uint8_t idx) {
        const std::lock_guard<std::mutex> guard(lock);
        current = get(idx);

        return current;
    }
    
    bool device_manager::add_new_device(const std::string &firmcode, const std::string &model, const std::string &manufacturer
        , const epocver ver) {
        const std::lock_guard<std::mutex> guard(lock);

        if (get(firmcode)) {
            return false;
        }

        devices.push_back({ ver, firmcode, manufacturer, model });

        return true;
    }

    bool device_manager::delete_device(const std::string &firmcode) {
        const std::lock_guard<std::mutex> guard(lock);

        auto result = std::find_if(devices.begin(), devices.end(),
            [&](const device &dvc) { return dvc.firmware_code == firmcode; });

        if (result != devices.end()) {
            devices.erase(result);
            return true;
        }

        return false;
    }

}