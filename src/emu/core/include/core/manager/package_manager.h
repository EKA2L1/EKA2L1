/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <common/types.h>

#include <map>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace loader {
        class sis_controller;
    }

	/*! \brief Managing apps. */
    namespace manager {
        using uid = uint32_t;

        struct app_info {
            std::u16string name;
            std::u16string vendor_name;
            std::u16string executable_name;
            uint8_t drive;

            uid id;
            epocver ver;
        };

        // A package manager, serves for managing apps
        // and implements part of HAL
        class package_manager {
            std::map<uid, app_info> c_apps;
            std::map<uid, app_info> e_apps;

            bool load_sdb(const std::string &path);
            bool write_sdb(const std::string &path);

            bool load_sdb_yaml(const std::string &path);
            bool write_sdb_yaml(const std::string &path);

            io_system *io;

            bool install_controller(loader::sis_controller *ctrl, uint8_t drv);

        public:
            package_manager() = default;
            package_manager(io_system *io)
                : io(io) { load_sdb_yaml("apps_registry.yml"); }

            bool installed(uid app_uid);

            size_t app_count() {
                return c_apps.size() + e_apps.size();
            }

            std::vector<app_info> get_apps_info() {
                std::vector<app_info> infos;

                for (auto const & [ c_drive, c_info ] : c_apps) {
                    infos.push_back(c_info);
                }

                for (auto const & [ e_drive, e_info ] : e_apps) {
                    infos.push_back(e_info);
                }

                return infos;
            }

            std::u16string app_name(uid app_uid);
            app_info info(uid app_uid);

            bool install_package(const std::u16string &path, uint8_t drive);
            bool uninstall_package(uid app_uid);

            std::string get_app_executable_path(uint32_t uid);
            std::string get_app_name(uint32_t uid);
        };
    }
}