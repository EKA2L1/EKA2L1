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

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <config/config.h>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eka2l1::config {    
    screen_buffer_sync_option get_screen_buffer_sync_option_from_string(std::string str) {
        str = common::lowercase_string(str);

        if (str == "preferred") {
            return screen_buffer_sync_option_preferred;
        }

        if (str == "on") {
            return screen_buffer_sync_option_on;
        }

        if (str == "off") {
            return screen_buffer_sync_option_off;
        }

        return screen_buffer_sync_option_preferred;
    }
    
    const char *get_string_from_screen_buffer_sync_option(const screen_buffer_sync_option opt) {
        switch (opt) {
        case screen_buffer_sync_option_preferred:
            return "preferred";

        case screen_buffer_sync_option_on:
            return "on";

        case screen_buffer_sync_option_off:
            return "off";

        default:
            break;
        }

        return nullptr;
    }

    midi_backend_type get_midi_backend_from_string(std::string str) {
        str = common::lowercase_string(str);

        if (str == "tsf") {
            return MIDI_BACKEND_TSF;
        }

        if (str == "minibae") {
            return MIDI_BACKEND_MINIBAE;
        }

        return MIDI_BACKEND_TSF;
    }

    const char *get_string_from_midi_backend(const midi_backend_type backend) {
        switch (backend) {
        case MIDI_BACKEND_TSF:
            return "tsf";

        case MIDI_BACKEND_MINIBAE:
            return "minibae";

        default:
            break;
        }

        return nullptr;
    }

    template <typename T, typename Q = T>
    void get_yaml_value(YAML::Node &config_node, const char *key, T *target_val, Q default_val) {
        try {
            *target_val = config_node[key].as<T>();
        } catch (...) {
            *target_val = std::move(default_val);
        }
    }

    template <typename T>
    void config_file_emit_single(YAML::Emitter &emitter, const char *name, T &val) {
        emitter << YAML::Key << name << YAML::Value << val;
    }

    template <typename T>
    void config_file_emit_vector(YAML::Emitter &emitter, const char *name, std::vector<T> &values) {
        emitter << YAML::Key << name << YAML::BeginSeq;

        for (const T &value : values) {
            emitter << value;
        }

        emitter << YAML::EndSeq;
    }

    void config_file_emit_keybind(YAML::Emitter &emitter, const keybind &kb) {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "source" << YAML::Value;
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "type" << YAML::Value << kb.source.type;
        emitter << YAML::Key << "data" << YAML::Value;
        if ((kb.source.type == config::KEYBIND_TYPE_KEY) || (kb.source.type == config::KEYBIND_TYPE_MOUSE)) {
            emitter << YAML::BeginMap;
            emitter << YAML::Key << "keycode" << YAML::Value << kb.source.data.keycode;
            emitter << YAML::EndMap;
        } else if (kb.source.type == config::KEYBIND_TYPE_CONTROLLER) {
            emitter << YAML::BeginMap;
            emitter << YAML::Key << "controller_id" << YAML::Value << kb.source.data.button.controller_id;
            emitter << YAML::Key << "button_id" << YAML::Value << kb.source.data.button.button_id;
            emitter << YAML::EndMap;
        } else {
            emitter << "error";
        }
        emitter << YAML::EndMap;
        emitter << YAML::Key << "target" << YAML::Value << kb.target;
        emitter << YAML::EndMap;
    }

    void keybind_profile::serialize(const std::string &file) {
        YAML::Emitter keybind_emitter;
        keybind_emitter << YAML::BeginSeq;
        for (const auto &kb : keybinds) {
            config_file_emit_keybind(keybind_emitter, kb);
        }
        keybind_emitter << YAML::EndSeq;

        common::wo_std_file_stream keybind_file(file, true);
        keybind_file.write(keybind_emitter.c_str(), keybind_emitter.size());
    }

    void keybind_profile::deserialize(const std::string &file) {
        YAML::Node keybind_node;
        try {
            common::ro_std_file_stream keybind_stream(file, true);
            if (!keybind_stream.valid()) {
                return;
            }

            std::string whole_config(keybind_stream.size(), ' ');
            keybind_stream.read(whole_config.data(), whole_config.size());

            keybind_node = YAML::Load(whole_config);
        } catch (...) {
            return;
        }

        keybinds.clear();
        for (size_t i = 0; i < keybind_node.size(); i++) {
            keybind kb;
            kb.target = keybind_node[i]["target"].as<std::uint32_t>();
            std::string source_type = keybind_node[i]["source"]["type"].as<std::string>();
            kb.source.type = source_type;
            if ((source_type == config::KEYBIND_TYPE_KEY) || (source_type == config::KEYBIND_TYPE_MOUSE)) {
                kb.source.data.keycode = keybind_node[i]["source"]["data"]["keycode"].as<std::uint32_t>();
            } else if (source_type == config::KEYBIND_TYPE_CONTROLLER) {
                kb.source.data.button.controller_id = keybind_node[i]["source"]["data"]["controller_id"].as<int>();
                kb.source.data.button.button_id = keybind_node[i]["source"]["data"]["button_id"].as<int>();
            }
            keybinds.emplace_back(kb);
        }
    }

    void state::serialize(const bool with_bindings) {
        audio_master_volume = common::clamp(0, 100, audio_master_volume);
        screen_buffer_sync_string = get_string_from_screen_buffer_sync_option(screen_buffer_sync);
        midi_backend_string = get_string_from_midi_backend(midi_backend);

        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

#define OPTION(name, variable, default) config_file_emit_single(emitter, #name, variable);
#include <config/options.inl>
#undef OPTION

        emitter << YAML::Key << "internet-bluetooth-friends" << YAML::Value;
        
        {
            emitter << YAML::BeginSeq;
            {
                for (std::size_t i = 0; i < friend_addresses.size(); i++) {
                    emitter << YAML::BeginMap;
                    {
                        emitter << YAML::Key << "address" << YAML::Value << friend_addresses[i].addr_;
                        emitter << YAML::Key << "port" << YAML::Value << friend_addresses[i].port_;
                    }
                    emitter<< YAML::EndMap;
                }
            }
            emitter << YAML::EndSeq;
        }

        emitter << YAML::EndMap;

        {
            common::wo_std_file_stream file("config.yml", true);
            file.write(emitter.c_str(), emitter.size());
        }

        if (with_bindings) {
            common::create_directories("bindings");
            keybinds.serialize(fmt::format("bindings/{}.yml", current_keybind_profile));
        }
    }

    void state::deserialize(const bool with_bindings) {
        YAML::Node node;

        try {
            common::ro_std_file_stream config_stream("config.yml", true);
            if (!config_stream.valid()) {
                return;
            }

            std::string whole_config(config_stream.size(), ' ');
            config_stream.read(whole_config.data(), whole_config.size());

            node = YAML::Load(whole_config);
        } catch (...) {
            serialize(false);
            return;
        }

#define OPTION(name, variable, default_value) get_yaml_value(node, #name, &variable, default_value);
#include <config/options.inl>
#undef OPTION

        try {
            auto net_bluetooth_friend_nodes = node["internet-bluetooth-friends"];
            
            for (auto friend_node: net_bluetooth_friend_nodes) {
                friend_address addr;
                addr.addr_ = friend_node["address"].as<std::string>();
                try {
                    addr.port_ = friend_node["port"].as<std::uint32_t>();
                } catch (...) {
                    addr.port_ = 35689;
                }
                friend_addresses.push_back(std::move(addr));
            }
        } catch (...) {
        }

        audio_master_volume = common::clamp(0, 100, audio_master_volume);
        screen_buffer_sync = get_screen_buffer_sync_option_from_string(screen_buffer_sync_string);
        midi_backend = get_midi_backend_from_string(midi_backend_string);

        if (!eka2l1::common::exists(hsb_bank_path)) {
            hsb_bank_path = "resources/defaultbank.hsb";
        }
        
        if (!eka2l1::common::exists(sf2_bank_path)) {
            hsb_bank_path = "resources/defaultbank.sf2";
        }

        if (with_bindings)
            keybinds.deserialize(fmt::format("bindings/{}.yml", current_keybind_profile));
    }
}
