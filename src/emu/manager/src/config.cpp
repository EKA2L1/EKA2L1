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

#include <manager/config.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace eka2l1::manager {
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

    void config_state::serialize() {
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
        config_file_emit_single(emitter, "log-code", log_code);
        config_file_emit_single(emitter, "enable-breakpoint-script", enable_breakpoint_script);
        config_file_emit_vector(emitter, "force-load", force_load_modules);
        config_file_emit_single(emitter, "cpu", cpu_backend);
        config_file_emit_single(emitter, "device", device);
        config_file_emit_single(emitter, "enable-gdb-stub", enable_gdbstub);
        config_file_emit_single(emitter, "data-storage", storage);
        config_file_emit_single(emitter, "display-size-x", display_size_x_pixs);
        config_file_emit_single(emitter, "display-size-y", display_size_y_pixs);
        config_file_emit_single(emitter, "ram-max", maximum_ram);
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

        emitter << YAML::EndMap;
        
        std::ofstream file("config.yml");
        file << emitter.c_str();
    }

    void config_state::deserialize() {
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
        get_yaml_value(node, "log-code", &log_code, false);
        get_yaml_value(node, "enable-breakpoint-script", &enable_breakpoint_script, false);
        get_yaml_value(node, "cpu", &cpu_backend, 0);
        get_yaml_value(node, "device", &device, 0);
        get_yaml_value(node, "enable-gdb-stub", &enable_gdbstub, false);
        get_yaml_value(node, "data-storage", &storage, "");
        get_yaml_value(node, "display_size_x", &display_size_x_pixs, 360);
        get_yaml_value(node, "display_size_y", &display_size_y_pixs, 640);
        get_yaml_value(node, "ram-max", &maximum_ram, static_cast<std::uint32_t>(common::MB(512)));
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

        try {
            YAML::Node force_loads_node = node["force-load"];

            for (auto force_load_node : force_loads_node) {
                force_load_modules.push_back(force_load_node.as<std::string>());
            }
        } catch (...) {
        }
    }

    const std::uint32_t config_state::get_hal_entry(const int hal_key) const {
        const hal_entry_key key = static_cast<hal_entry_key>(hal_key);

        switch (key) {
        case hal_entry_key::display_screen_x_pixels: {
            return display_size_x_pixs;
        }

        case hal_entry_key::display_screen_y_pixels: {
            return display_size_y_pixs;
        }

        case hal_entry_key::cpu: {
            // 0 = ARM, 1 = x86, 2 = MCORE
            return 0;
        }

        case hal_entry_key::ram: {
            return maximum_ram;
        }

        default: {
            LOG_ERROR("Unimplement HAL variable, key = 0x{:X}, return 0 by default", hal_key);
            break;
        }
        }

        return 0;
    }
}
