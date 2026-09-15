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
#include <common/configure.h>
#include <common/platform.h>

#include <config/config.h>
#include <fstream>
#include <yaml-cpp/yaml.h>

#if EKA2L1_PLATFORM(WIN32)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace eka2l1::config {
    std::string normalize_host_name(std::string name) {
        const auto first = name.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        name = name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
        if (name.back() == '.') {
            name.pop_back();
        }
        return common::lowercase_string(name);
    }

    bool valid_host_name(const std::string &name) {
        if (name.empty() || name.size() > 253) {
            return false;
        }
        std::size_t label_size = 0;
        for (const char ch : name) {
            if (ch == '.') {
                if (!label_size) {
                    return false;
                }
                label_size = 0;
            } else if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') && ++label_size <= 63) {
                continue;
            } else {
                return false;
            }
        }
        return label_size != 0;
    }

    bool valid_host_pattern(const std::string &pattern) {
        return valid_host_name(pattern) || (pattern.starts_with("*.") && valid_host_name(pattern.substr(2)));
    }

    bool numeric_host_address(const std::string &address) {
        if (address.find('\0') != std::string::npos) {
            return false;
        }
        in_addr ipv4;
        in6_addr ipv6;
        return inet_pton(AF_INET, address.c_str(), &ipv4) == 1
            || inet_pton(AF_INET6, address.c_str(), &ipv6) == 1;
    }

    std::optional<host_target> parse_host_target(const std::string &target) {
        const auto normalized = normalize_host_name(target);
        if (normalized.empty() || normalized.find('\0') != std::string::npos) {
            return std::nullopt;
        }

        std::string hostname = normalized;
        std::string port_text;
        if (normalized.front() == '[') {
            const auto bracket = normalized.find(']');
            if (bracket == std::string::npos || bracket == 1 || bracket + 1 >= normalized.size()
                || normalized[bracket + 1] != ':') {
                return std::nullopt;
            }
            hostname = normalized.substr(1, bracket - 1);
            port_text = normalized.substr(bracket + 2);
        } else if (!numeric_host_address(normalized)) {
            const auto colon = normalized.rfind(':');
            if (colon != std::string::npos) {
                hostname = normalized.substr(0, colon);
                port_text = normalized.substr(colon + 1);
            }
        }

        std::optional<std::uint16_t> port;
        if (!port_text.empty()) {
            if (port_text.size() > 5 || port_text.find_first_not_of("0123456789") != std::string::npos) {
                return std::nullopt;
            }
            const auto value = std::stoul(port_text);
            if (!value || value > 65535) {
                return std::nullopt;
            }
            port = static_cast<std::uint16_t>(value);
        } else if (hostname != normalized) {
            return std::nullopt;
        }

        if (!numeric_host_address(hostname)
            && (!valid_host_name(hostname) || hostname.find_first_not_of("0123456789.") == std::string::npos)) {
            return std::nullopt;
        }
        return host_target{hostname, port};
    }

    bool valid_host_target(const std::string &target) {
        return parse_host_target(target).has_value();
    }

    std::optional<host_target> state::host_override(const std::string &hostname) const {
        const std::string domain = normalize_host_name(hostname);
        const std::string *wildcard_target = nullptr;
        std::size_t wildcard_size = 0;
        for (const auto &[host, address] : hosts) {
            const auto pattern = normalize_host_name(host);
            if (pattern == domain) {
                return parse_host_target(address);
            }
            const auto suffix = pattern.starts_with("*.") ? pattern.substr(1) : std::string{};
            if (!suffix.empty() && domain.size() > suffix.size()
                && domain.compare(domain.size() - suffix.size(), suffix.size(), suffix) == 0
                && pattern.size() > wildcard_size) {
                wildcard_target = &address;
                wildcard_size = pattern.size();
            }
        }
        return wildcard_target ? parse_host_target(*wildcard_target) : std::nullopt;
    }

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

        if (current_mmc_id.empty()) {
            current_mmc_id = mmc_id;
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

#if BUILD_FOR_USER
        // If not yet been modified by user, see extensive logging option
        if (log_filter.empty() || (log_filter == DEFAULT_LOG_FILTERING)) {
            if (extensive_logging) {
                log_filter = LOG_FILTER_DEBUG_PRESET;
            } else {
                log_filter = LOG_FILTER_NORMAL_USE_PRESET;
            }
        }
#endif
    }
}
