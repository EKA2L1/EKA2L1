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

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace eka2l1::epoc {
    using uid = std::uint32_t;
}

namespace eka2l1::config {
    struct state;

    struct app_setting {
    public:
        std::uint32_t fps;
        std::uint32_t time_delay;

        int screen_rotation;

        float screen_upscale;
        std::uint32_t screen_upscale_method;
        std::string filter_shader_path;
        bool child_inherit_setting;

        // Hack through game crashed around T9 log. Force *.dll and *.rsc global search to go through z:\system\t9ldb 
        // Designed for ONE (N-Gage)
        bool t9_bypass_hack;

        explicit app_setting();
    };

    class app_settings {
        std::map<epoc::uid, app_setting> settings_;
        config::state *conf_;

    protected:
        bool load_all_settings();
        void save_setting(const epoc::uid id, const app_setting &setting);

    public:
        explicit app_settings(config::state *conf);

        app_setting *get_setting(const epoc::uid app_uid);

        /**
         * @brief       Add new setting or replace an existing setting for current application
         * 
         * @param       app_uid         The UID3 of the app to add or replace the setting.
         * @param       setting_to_add  The setting intended for this application with the associated UID.
         * 
         * @returns     True if the setting is added and unique.
         * @see         get_setting
         */
        bool add_or_replace_setting(const epoc::uid app_uid, const app_setting &setting_to_add);

        void update_setting(const epoc::uid app_uid);
    };
}
