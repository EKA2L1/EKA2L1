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

        config_file_emit_single(emitter, "ui-scale", ui_scale);
        config_file_emit_single(emitter, "bkg-alpha", bkg_transparency);
        config_file_emit_single(emitter, "bkg-path", bkg_path);
        config_file_emit_single(emitter, "font", font_path);
        config_file_emit_single(emitter, "log-read", log_read);
        config_file_emit_single(emitter, "log-write", log_write);
        config_file_emit_single(emitter, "log-ipc", log_ipc);
        config_file_emit_single(emitter, "log-svc", log_svc);
        config_file_emit_single(emitter, "log-passed", log_passed);
        config_file_emit_single(emitter, "log-exports", log_exports);
        config_file_emit_single(emitter, "cpu", cpu_backend);
        config_file_emit_single(emitter, "device", device);
        config_file_emit_single(emitter, "language", language);
        config_file_emit_single(emitter, "emulator-language", language);
        config_file_emit_single(emitter, "enable-gdb-stub", enable_gdbstub);
        config_file_emit_single(emitter, "data-storage", storage);
        config_file_emit_single(emitter, "gdb-port", gdb_port);
        config_file_emit_single(emitter, "enable-srv-ecom", enable_srv_ecom);
        config_file_emit_single(emitter, "enable-srv-cenrep", enable_srv_cenrep);
        config_file_emit_single(emitter, "enable-srv-backup", enable_srv_backup);
        config_file_emit_single(emitter, "enable-srv-install", enable_srv_install);
        config_file_emit_single(emitter, "enable-srv-rights", enable_srv_rights);
        config_file_emit_single(emitter, "enable-srv-sa", enable_srv_sa);
        config_file_emit_single(emitter, "enable-srv-drm", enable_srv_drm);
        config_file_emit_single(emitter, "enable-srv-eikappui", enable_srv_eikapp_ui);
        config_file_emit_single(emitter, "enable-srv-akn-icon", enable_srv_akn_icon);
        config_file_emit_single(emitter, "enable-srv-akn-skin", enable_srv_akn_skin);
        config_file_emit_single(emitter, "enable-srv-cdl", enable_srv_cdl);
        config_file_emit_single(emitter, "enable-srv-socket", enable_srv_socket);
        config_file_emit_single(emitter, "fbs-enable-compression-queue", fbs_enable_compression_queue);
        config_file_emit_single(emitter, "accurate-ipc-timing", accurate_ipc_timing);
        config_file_emit_single(emitter, "enable-btrace", enable_btrace);
        config_file_emit_single(emitter, "stop-warn-touchscreen-disabled", stop_warn_touch_disabled);
        config_file_emit_single(emitter, "dump-imb-range-code", dump_imb_range_code);
        config_file_emit_single(emitter, "hide-mouse-in-screen-space", hide_mouse_in_screen_space);

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

        get_yaml_value(node, "ui-scale", &ui_scale, 1.0f);
        get_yaml_value(node, "bkg-alpha", &bkg_transparency, 129);
        get_yaml_value(node, "bkg-path", &bkg_path, "");
        get_yaml_value(node, "font", &font_path, "");
        get_yaml_value(node, "log-read", &log_read, false);
        get_yaml_value(node, "log-write", &log_write, false);
        get_yaml_value(node, "log-ipc", &log_ipc, false);
        get_yaml_value(node, "log-svc", &log_svc, false);
        get_yaml_value(node, "log-passed", &log_passed, false);
        get_yaml_value(node, "log-exports", &log_exports, false);
        get_yaml_value(node, "cpu", &cpu_backend, 0);
        get_yaml_value(node, "device", &device, 0);
        get_yaml_value(node, "language", &language, -1);
        get_yaml_value(node, "emulator-language", &language, -1);
        get_yaml_value(node, "enable-gdb-stub", &enable_gdbstub, false);
        get_yaml_value(node, "data-storage", &storage, "");
        get_yaml_value(node, "gdb-port", &gdb_port, 24689);
        get_yaml_value(node, "enable-srv-ecom", &enable_srv_ecom, true);
        get_yaml_value(node, "enable-srv-cenrep", &enable_srv_cenrep, true);
        get_yaml_value(node, "enable-srv-backup", &enable_srv_backup, true);
        get_yaml_value(node, "enable-srv-install", &enable_srv_install, true);
        get_yaml_value(node, "enable-srv-rights", &enable_srv_rights, true);
        get_yaml_value(node, "enable-srv-sa", &enable_srv_sa, true);
        get_yaml_value(node, "enable-srv-drm", &enable_srv_drm, true);
        get_yaml_value(node, "enable-srv-eikappui", &enable_srv_eikapp_ui, true);
        get_yaml_value(node, "enable-srv-akn-icon", &enable_srv_akn_icon, true);
        get_yaml_value(node, "enable-srv-akn-skin", &enable_srv_akn_skin, true);
        get_yaml_value(node, "enable-srv-cdl", &enable_srv_cdl, true);
        get_yaml_value(node, "enable-srv-socket", &enable_srv_socket, false);
        get_yaml_value(node, "fbs-enable-compression-queue", &fbs_enable_compression_queue, false);
        get_yaml_value(node, "accurate-ipc-timing", &accurate_ipc_timing, false);
        get_yaml_value(node, "enable-btrace", &enable_btrace, false);
        get_yaml_value(node, "stop-warn-touchscreen-disabled", &stop_warn_touch_disabled, false);
        get_yaml_value(node, "dump-imb-range-code", &dump_imb_range_code, false);
        get_yaml_value(node, "hide-mouse-in-screen-space", &hide_mouse_in_screen_space, false);

        YAML::Node keybind_node;
        try {
            keybind_node = YAML::LoadFile("keybind.yml");
        } catch (...) {
            return;
        }

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
