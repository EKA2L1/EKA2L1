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
#include <common/log.h>

#include <config/config.h>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eka2l1::config {
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

    void state::serialize() {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        #define OPTION(name, variable, default) config_file_emit_single(emitter, #name, variable);
        #include <config/options.inl>
        #undef OPTION

        emitter << YAML::EndMap;

        std::ofstream file("config.yml");
        file << emitter.c_str();
        file.close();

        YAML::Emitter keybind_emitter;
        keybind_emitter << YAML::BeginSeq;
        for (const auto &kb : keybinds) {
            config_file_emit_keybind(keybind_emitter, kb);
        }
        keybind_emitter << YAML::EndSeq;

        std::ofstream keybind_file("keybind.yml");
        keybind_file << keybind_emitter.c_str();
        keybind_file.close();
    }

    void state::deserialize() {
        YAML::Node node;

        try {
            node = YAML::LoadFile("config.yml");
        } catch (...) {
            serialize();
            return;
        }

        #define OPTION(name, variable, default_value) get_yaml_value(node, #name, &variable, default_value);
        #include <config/options.inl>
        #undef OPTION

        YAML::Node keybind_node;
        try {
            keybind_node = YAML::LoadFile("keybind.yml");
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
}
