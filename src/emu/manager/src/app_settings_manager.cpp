/*
 * Copyright (c) 2020 EKA2L1 Team.
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
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <config/config.h>

#include <manager/app_settings_manager.h>
#include <fstream>

#include <yaml-cpp/yaml.h>

namespace eka2l1::manager {
    app_setting::app_setting()
        : fps(30)
        , time_delay(0) {
    }

    static void serialize_app_setting(YAML::Emitter &emitter, const app_setting &setting) {
        #define SETTING(name, variable, default_var)  emitter << YAML::Key << #name << YAML::Value << setting.variable;
        #include "app_settings.inl"
        #undef SETTING
    }

    static void deserialize_app_setting(YAML::Node &node, app_setting &setting) {
        #define SETTING(name, variable, default_val)                                         \
            try {                                                                            \
                setting.variable = node[#name].as<decltype(app_setting::variable)>();        \
            } catch (...) {                                                                  \
                setting.variable = default_val;                                              \
            }

        #include "app_settings.inl"
        #undef SETTING
    }

    app_settings::app_settings(config::state *conf)
        : conf_(conf) {
        load_all_settings();
    }

    app_setting *app_settings::get_setting(const epoc::uid app_uid) {
        auto ite = settings_.find(app_uid);

        if (ite == settings_.end()) {
            return nullptr;
        }

        return &(ite->second);
    }

    static const char *COMPAT_DIR_PATH = "\\compat\\";

    bool app_settings::load_all_settings() {
        common::dir_iterator setting_folder(eka2l1::add_path(conf_->storage, COMPAT_DIR_PATH));
        setting_folder.detail = true;

        common::dir_entry setting_entry;

        while (setting_folder.next_entry(setting_entry) == 0) {
            if ((setting_entry.type == common::FILE_REGULAR) && common::lowercase_string(eka2l1::path_extension(setting_entry.name)) == ".yml") {
                const common::pystr fname = eka2l1::replace_extension(eka2l1::filename(setting_entry.name), "");
                const epoc::uid uid = fname.as_int<std::uint32_t>();

                app_setting target_setting;
                YAML::Node the_node;

                try {
                    the_node = YAML::LoadFile(setting_entry.name);
                } catch (std::exception &exc) {
                    LOG_ERROR("Encountering error while loading app setting {}. Error message: {}", fname.std_str(),
                        exc.what());
                    continue;
                }

                deserialize_app_setting(the_node, target_setting);
                settings_.emplace(uid, target_setting);
            }
        }

        return true;
    }

    void app_settings::save_setting(const epoc::uid id, const app_setting &setting) {
        std::ofstream stream(eka2l1::add_path(eka2l1::add_path(conf_->storage, COMPAT_DIR_PATH), fmt::format("0x{:X}.yml", id)));
        YAML::Emitter emitter;

        serialize_app_setting(emitter, setting);
        stream << emitter.c_str();
    }

    bool app_settings::add_or_replace_setting(const epoc::uid app_uid, const app_setting &setting_to_add) {
        auto ite = settings_.find(app_uid);

        if (ite == settings_.end()) {
            settings_.emplace(app_uid, setting_to_add);
        } else {
            ite->second = setting_to_add;
        }

        save_setting(app_uid, setting_to_add);
        return true;
    }
}