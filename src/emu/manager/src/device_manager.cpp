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

#include <config/config.h>
#include <manager/device_manager.h>
#include <yaml-cpp/yaml.h>

#include <common/algorithm.h>
#include <common/dynamicfile.h>
#include <common/path.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace eka2l1::manager {
    void get_recommended_stat_for_device(const epocver ver, std::uint16_t &time_delay_us) {
        time_delay_us = 100;

        if (ver >= epocver::eka2) {
            time_delay_us = 0;
        }
    }

    void device_manager::load_devices() {
        YAML::Node devices_node{};

        {    
            const std::lock_guard<std::mutex> guard(lock);
            devices.clear();
        }

        try {
            devices_node = std::move(YAML::LoadFile(add_path(conf->storage, "devices.yml")));
        } catch (YAML::Exception exception) {
            return;
        }

        for (auto device_node : devices_node) {
            const std::string firmcode = device_node.first.as<std::string>();
            const std::string manufacturer = device_node.second["manufacturer"].as<std::string>();
            const std::string model = device_node.second["model"].as<std::string>();

            const std::string plat_ver = device_node.second["platver"].as<std::string>();
            epocver ver = string_to_epocver(plat_ver.c_str());

            std::uint16_t time_now_sleep_duration = 0;
            get_recommended_stat_for_device(ver, time_now_sleep_duration);

            if (YAML::Node time_to_sleep = device_node.second["time-delay-us"]) {
                time_now_sleep_duration = time_to_sleep.as<std::uint16_t>();
            }

            std::uint32_t machine_uid = 0;
            
            try {
                machine_uid = device_node.second["machine-uid"].as<int>();
            } catch (YAML::Exception exception) {
                machine_uid = 0;
            }

            add_new_device(firmcode, model, manufacturer, ver, machine_uid, time_now_sleep_duration);
        }
    }

    void device_manager::save_devices() {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        for (const auto &device : devices) {
            emitter << YAML::Key << device.firmware_code;
            emitter << YAML::Value << YAML::BeginMap;

            emitter << YAML::Key << "platver" << YAML::Value << epocver_to_string(device.ver);
            emitter << YAML::Key << "manufacturer" << YAML::Value << device.manufacturer;
            emitter << YAML::Key << "firmcode" << YAML::Value << device.firmware_code;
            emitter << YAML::Key << "model" << YAML::Value << device.model;
            emitter << YAML::Key << "machine-uid" << YAML::Value << device.machine_uid;
            emitter << YAML::Key << "time-delay-us" << YAML::Value << device.time_delay_us;

            emitter << YAML::EndMap;
        }

        emitter << YAML::EndMap;
        std::ofstream outdevicefile(add_path(conf->storage, "devices.yml"));

        outdevicefile << emitter.c_str();

        return;
    }

    device_manager::device_manager(config::state *conf)
        : conf(conf)
        , current(nullptr) {
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

    void device_manager::import_device_config_to_global(device *dvc) {
        conf->time_getter_sleep_us = dvc->time_delay_us;
    }
    
    bool device_manager::set_current(const std::string &firmcode) {
        const std::lock_guard<std::mutex> guard(lock);
        current = get(firmcode);
        
        import_device_config_to_global(current);
        return current;
    }

    bool device_manager::set_current(const std::uint8_t idx) {
        const std::lock_guard<std::mutex> guard(lock);
        current = get(idx);

        import_device_config_to_global(current);
        return current;
    }

    void device_manager::set_time_delay_in_us(const std::uint16_t delay_us) {
        const std::lock_guard<std::mutex> guard(lock);
        current->time_delay_us = delay_us;
        conf->time_getter_sleep_us = delay_us;
    }

    bool device_manager::add_new_device(const std::string &firmcode, const std::string &model, const std::string &manufacturer, const epocver ver, const std::uint32_t machine_uid,
        const std::uint16_t time_delay_us) {
        const std::lock_guard<std::mutex> guard(lock);

        if (get(firmcode)) {
            return false;
        }

        std::vector<int> languages;
        int default_language = -1;
        const auto lang_path = eka2l1::add_path(conf->storage, "/drives/z/" + common::lowercase_string(firmcode) + (ver < epocver::eka2 ?
            "/system/bootdata/languages.txt" : "/resource/bootdata/languages.txt"));
        common::dynamic_ifile ifile(lang_path);
        if (ifile.fail()) {
            return false;
        }
        std::string line;
        while (ifile.getline(line)) {
            if (line == "" || line[0] == '\0')
                break;
            const int lang_code = std::stoi(line);
            if (line.find_first_of(",d") != std::string::npos) {
                default_language = lang_code;
            }
            languages.push_back(lang_code);
        }
        if (default_language == -1)
            default_language = languages[0];

        device dvc = { ver, firmcode, manufacturer, model };
        dvc.languages = languages;
        dvc.default_language_code = default_language;
        dvc.machine_uid = machine_uid;
        dvc.time_delay_us = time_delay_us;

        devices.push_back(dvc);
        return true;
    }

    bool device_manager::delete_device(const std::string &firmcode) {
        const std::lock_guard<std::mutex> guard(lock);

        auto result = std::find_if(devices.begin(), devices.end(),
            [&](const device &dvc) { return dvc.firmware_code == firmcode; });

        if (result != devices.end()) {
            devices.erase(result);
            save_devices();
            return true;
        }

        return false;
    }
}