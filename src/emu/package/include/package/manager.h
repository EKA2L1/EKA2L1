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

#include <atomic>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace loader {
        struct sis_controller;

        using show_text_func = std::function<bool(const char *, const bool)>;
        using choose_lang_func = std::function<int(const int *langs, const int count)>;
        using var_value_resolver_func = std::function<std::int32_t(std::uint32_t)>;
    }

    namespace config {
        struct state;
    }

    /*! \brief Managing apps. */
    namespace manager {
        using uid = uint32_t;

        struct package_info {
            std::u16string name;
            std::u16string vendor_name;
            drive_number drive;

            uid id;
        };

        // A package manager, serves for managing packages
        class packages {
            std::vector<package_info> pkgs;

            bool load_sdb_yaml(const std::string &path);
            bool write_sdb_yaml(const std::string &path);

            io_system *sys;
            config::state *conf;

            bool install_controller(loader::sis_controller *ctrl, drive_number drv);
            void add_package(package_info &pkg);

        public:
            mutable std::mutex lockdown;

            loader::show_text_func show_text;
            loader::choose_lang_func choose_lang;

            explicit packages(io_system *sys, config::state *conf);

            bool installed(const uid pkg_uid);

            const std::size_t package_count() const {
                return pkgs.size();
            }

            // No thread safe
            const package_info *package(const std::size_t idx) const;
            void get_file_bucket(const manager::uid pkg_uid, std::vector<std::string> &paths);
            bool add_to_file_bucket(const uid package_uid, const std::string &path);
            void delete_files_and_bucket(const uid package_uid);

            bool install_package(const std::u16string &path, const drive_number drive,
                std::atomic<int> &progress);

            bool uninstall_package(const uid app_uid);
        };
    }
}
