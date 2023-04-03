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

#include <common/buffer.h>
#include <config/config.h>
#include <system/devices.h>
#include <yaml-cpp/yaml.h>

#include <common/algorithm.h>
#include <common/dynamicfile.h>
#include <common/log.h>
#include <common/path.h>
#include <common/types.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace eka2l1 {
    static std::map<std::string, std::uint32_t> DEVICE_UID_MAP = {
        { "nem-4", 0x101F8C19 }, // Nokia N-Gage
        { "rh-29", 0x101FB2B1 }, // Nokia N-Gage QD
        { "rm-325", 0x101FB3DD }, // Nokia 6600
        { "rm-51", 0x10200F97 }, // Nokia 3230
        { "rm-25", 0x101FB3F4 }, // Nokia 6260
        { "nhl-12", 0x101F3EE3 }, // Nokia 6620
        { "rh-67", 0x101FB3F3 }, // Nokia 6670
        { "rh-51", 0x101FB3F3 }, // Nokia 7670
        { "rm-1", 0x101FBB55 }, // Nokia 6630
        { "rm-36", 0x10200F99 }, // Nokia 6680
        { "rm-57", 0x10200F9C }, // Nokia 6681
        { "rm-58", 0x10200F9B }, // Nokia 6682
        { "rm-84", 0x10200F9A }, // Nokia N70
        { "rm-99", 0x10200F9A }, // Nokia N70-5
        { "rm-180", 0x10200F9A }, // Nokia N72
        { "rm-42", 0x10200F98 }, // Nokia N90
        { "rm-38", 0x200005F8 }, // Nokia 3250
        { "rm-86", 0x20000602 }, // Nokia 5500 Sport
        { "rm-170", 0x20002495 }, // Nokia E50
        { "rm-171", 0x20002495 }, // Nokia E50-2
        { "rm-49", 0x20001856 }, // Nokia E60
        { "rm-89", 0x20001858 }, // Nokia E61
        { "rm-227", 0x20002D7F }, // Nokia E61i
        { "rm-88", 0x20001859 }, // Nokia E62
        { "rm-208", 0x20000604 }, // Nokia E65
        { "rm-24", 0x20001857 }, // Nokia E70
        { "rm-67", 0x20001857 }, // Nokia N71
        { "rm-132", 0x20001857 }, // Nokia N73
        { "rm-133", 0x20001857 }, // Nokia N73-2
        { "rm-128", 0x200005FE }, // Nokia N75
        { "rm-194", 0x20000601 }, // Nokia N77
        { "rm-92", 0x200005F9 }, // Nokia N80
        { "rm-158", 0x200005FC }, // Nokia N91
        { "rm-100", 0x200005FA }, // Nokia N92
        { "rm-55", 0x20000600 }, // Nokia N93
        { "rm-156", 0x20000605 }, // Nokia N93i
        { "rm-157", 0x20000605 }, // Nokia N93i
        { "rm-230", 0x20002D7C }, // Nokia 5700 XpressMusic
        { "rm-122", 0x20002D7B }, // Nokia 6110 Navigator
        { "rm-243", 0x20002D7E }, // Nokia 6120 Classic
        { "rm-176", 0x20000606 }, // Nokia 6290
        { "rm-426", 0x20002498 }, // Nokia E51
        { "rm-407", 0x2000249b }, // Nokia E71
        { "rm-493", 0x2000249b }, // Nokia E71??
        { "ra-6", 0x20002496 }, // Nokia N90
        { "rm-135", 0x2000060A }, // Nokia N76
        { "rm-149", 0x2000060A },
        { "rm-256", 0x20002D83 }, // Nokia N81
        { "rm-179", 0x20002D83 },
        { "rm-223", 0x20002D83 },
        { "rm-313", 0x20002D85 }, // Nokia N82
        { "rm-314", 0x20002D85 },
        { "rm-159", 0x2000060B }, // Nokia N95
        { "rm-160", 0x2000060B },
        { "rm-245", 0x2000060B },
        { "rm-320", 0x20002D84 }, // Nokia N95 8GB
        { "rm-321", 0x2000060B },
        { "rm-409", 0x2000DA5A }, // Nokia 5320 XpressMusic
        { "rm-431", 0x2000DA61 }, // Nokia 5630 XpressMusic
        { "rm-465", 0x20014DD3 }, // Nokia 5730 XpressMusic
        { "rm-367", 0x2000DA54 }, // Nokia 6210 Navigator
        { "rm-328", 0x2000DA52 }, // Nokia 6220 Classic
        { "rm-324", 0x2000DA57 }, // Nokia 6650
        { "rm-400", 0x2000DA57 },
        { "rm-235", 0x20002D81 }, // Nokia N78
        { "rm-236", 0x20002D81 },
        { "rm-342", 0x20002D81 },
        { "rm-346", 0x2000DA64 }, // Nokia N79
        { "rm-348", 0x2000DA64 },
        { "rm-349", 0x2000DA64 },
        { "rm-350", 0x2000DA64 },
        { "rm-333", 0x20002D86 }, // Nokia N85
        { "rm-334", 0x20002D86 },
        { "rm-335", 0x20002D86 },
        { "rm-297", 0x20002D82 }, // Nokia N96
        { "rm-472", 0x20002D82 },
        { "rm-247", 0x20002D82 },
        { "rm-632", 0x20024100 }, // Nokia E5-00
        { "rm-634", 0x20024100 },
        { "rm-627", 0x20029A76 }, // Nokia X5-01
        { "rm-356", 0x2000DA56 }, // Nokia 5800 XpressMusic
        { "rm-428", 0x2000DA56 },
        { "rm-625", 0x20024104 }, // Nokia 5228
        { "rm-588", 0x20023763 }, // Nokia 5230.1
        { "rm-594", 0x20023764 }, // Nokia 5230.2
        { "rm-629", 0x20024105 }, // Nokia 5230.3
        { "rm-720", 0x2002C130 }, // Nokia C5-04
        { "rm-612", 0x2002BF91 }, // Nokia C6-00
        { "rm-505", 0x20014DDD }, // Nokia N97
        { "rm-507", 0x20014DDD },
        { "rm-506", 0x20014DDE },
        { "rm-553", 0x20023766 }, // Nokia N97 Mini
        { "rm-555", 0x20023766 },
        { "rm-559", 0x200227DD }, // Nokia X6-00
        { "rm-551", 0x200227DE },
        { "rm-552", 0x200227DF },
        { "rm-718", 0x2002376A }, // Nokia C6-01
        { "rm-675", 0x2002BF92 }, // Nokia C7-00
        { "rm-691", 0x2002BF92 }, // Nokia C7-01
        { "rm-626", 0x2002BF96 }, // Nokia E7-00
        { "rm-596", 0x20029A73 }, // Nokia N8
        { "rm-707", 0x20029A6F }, // Nokia X7-00
        { "rm-609", 0x20023767 }, // Nokia E6-00
        { "rm-749", 0x20035560 }, // Nokia Oro
        { "rm-807", 0x2003AB64 }, // Nokia 808 PureView
        { "rm-774", 0x2002BF94 }, // Nokia 701
        { "rm-670", 0x2002C12C }, // Nokia 700
        { "rm-779", 0x20035565 }, // Nokia 603
        { "rm-750", 0x20035566 }, // Nokia 500
    };

    static std::array<std::string, 3> S80_DEVICES_FIRMCODE = {
        "RAE-6", 
        "RA-2",
        "RA-8"
    };

    void device::init_flags() {
        cached_flags_ = 0;
        flag_inited_ = true;

        for (const std::string &device: S80_DEVICES_FIRMCODE) {
            if (common::compare_ignore_case(device.c_str(), firmware_code.c_str()) == 0) {
                cached_flags_ |= DEVICE_FLAG_S80;
                break;
            }
        }
    }
    
    bool device::is_s80() {
        if (!flag_inited_) {
            init_flags();
        }

        return cached_flags_ & DEVICE_FLAG_S80;
    }

    void device_manager::load_devices() {
        YAML::Node devices_node{};

        {
            const std::lock_guard<std::mutex> guard(lock);
            devices.clear();
        }

        try {
            common::ro_std_file_stream devices_stream(add_path(conf->storage, "devices.yml"), true);
            if (!devices_stream.valid()) {
                LOG_ERROR(SYSTEM, "Devices file not found (or is invalid)!");
                return;
            }

            std::string whole_config(devices_stream.size(), ' ');
            devices_stream.read(whole_config.data(), whole_config.size());

            devices_node = YAML::Load(whole_config);
        } catch (YAML::Exception exception) {
            return;
        }

        for (auto device_node : devices_node) {
            const std::string firmcode = device_node.first.as<std::string>();
            const std::string manufacturer = device_node.second["manufacturer"].as<std::string>();
            const std::string model = device_node.second["model"].as<std::string>();

            const std::string plat_ver = device_node.second["platver"].as<std::string>();
            epocver ver = string_to_epocver(plat_ver.c_str());

            std::uint32_t machine_uid = 0;

            try {
                machine_uid = device_node.second["machine-uid"].as<int>();
            } catch (YAML::Exception exception) {
                machine_uid = 0;
            }

            if (machine_uid == 0) {
                auto ite = DEVICE_UID_MAP.find(common::lowercase_string(firmcode));

                if (ite != DEVICE_UID_MAP.end()) {
                    machine_uid = ite->second;
                }
            }

            add_new_device(firmcode, model, manufacturer, ver, machine_uid);
        }

        // Save any additions we add it during deserialize
        save_devices();
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

            emitter << YAML::EndMap;
        }

        emitter << YAML::EndMap;

        common::wo_std_file_stream outdevicefile(add_path(conf->storage, "devices.yml"), true);
        outdevicefile.write(emitter.c_str(), emitter.size());

        return;
    }

    device_manager::device_manager(config::state *conf)
        : conf(conf)
        , current_index(0) {
        load_devices();
    }

    device_manager::~device_manager() {
    }

    void device_manager::clear() {
        const std::lock_guard<std::mutex> guard(lock);
        devices.clear();
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
        auto result = std::find_if(devices.begin(), devices.end(),
            [&](const device &dvc) { return dvc.firmware_code == firmcode; });

        if (result != devices.end()) {
            current_index = static_cast<std::int32_t>(std::distance(devices.begin(), result));
            return true;
        }

        return false;
    }

    bool device_manager::set_current(const std::uint8_t idx) {
        const std::lock_guard<std::mutex> guard(lock);
        if (idx >= devices.size()) {
            return false;
        }

        current_index = static_cast<std::int32_t>(idx);
        return true;
    }

    add_device_error device_manager::add_new_device(const std::string &firmcode, const std::string &model, const std::string &manufacturer, const epocver ver, const std::uint32_t machine_uid) {
        const std::lock_guard<std::mutex> guard(lock);

        if (get(firmcode)) {
            LOG_ERROR(SYSTEM, "Device already installed ({})!", firmcode);
            return add_device_existed;
        }

        std::vector<int> languages;
        int default_language = -1;
        const auto lang_path = eka2l1::add_path(conf->storage, "/drives/z/" + common::lowercase_string(firmcode) + (ver < epocver::eka2 ? "/system/bootdata/languages.txt" : "/resource/bootdata/languages.txt"));
        common::dynamic_ifile ifile(lang_path);
        if (ifile.fail()) {
            LOG_ERROR(SYSTEM, "Fail to load languages.txt file! (Searched path: {}).", lang_path);
        } else {
            ifile.set_ucs2(0); // Nokia 7610 isn't having a BOM
            std::string line;
            while (ifile.getline(line)) {
                if ((line == "") || (line[0] == '\0'))
                    break;

                if ((line == "\r") || (line == "\n") || (line == "\r\n")) {
                    continue;
                }

                const int lang_code = std::stoi(line);
                if (line.find_first_of(",d") != std::string::npos) {
                    default_language = lang_code;
                }
                languages.push_back(lang_code);
            }
        }
        if (languages.empty()) {
            LOG_WARN(SYSTEM, "No language is specified, adding English to prevent empty");
            languages.push_back(static_cast<int>(language::en));
        }
        if (default_language == -1)
            default_language = languages[0];

        device dvc(ver, firmcode, manufacturer, model);
        dvc.languages = languages;
        dvc.default_language_code = default_language;
        dvc.machine_uid = machine_uid;

        devices.push_back(dvc);

        return add_device_none;
    }

    bool device_manager::delete_device(const std::string &firmcode) {
        const std::lock_guard<std::mutex> guard(lock);

        auto result = std::find_if(devices.begin(), devices.end(),
            [&](const device &dvc) { return dvc.firmware_code == firmcode; });

        if (result != devices.end()) {
            if (current_index > std::distance(devices.begin(), result)) {
                current_index--;
            }

            devices.erase(result);
            save_devices();

            if (current_index >= devices.size()) {
                current_index = static_cast<std::int32_t>(devices.size()) - 1;
            }

            return true;
        }

        return false;
    }
}
