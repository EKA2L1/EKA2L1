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
#include <package/registry.h>

#include <atomic>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace loader {
        struct sis_controller;
        struct sis_registry_tree;

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

        struct controller_info {
            std::uint8_t *data_;
            std::size_t size_;
        };

        using object_map_type = std::multimap<uid, package::object>;

        // A package manager, serves for managing packages
        class packages {
            object_map_type objects_;
            drive_number residing_;

            io_system *sys;
            config::state *conf;

        protected:
            void traverse_tree_and_add_packages(loader::sis_registry_tree &tree);
            void install_sis_stubs();

        public:
            mutable std::mutex lockdown;

            loader::show_text_func show_text;
            loader::choose_lang_func choose_lang;
            loader::var_value_resolver_func var_resolver;

            explicit packages(io_system *sys, config::state *conf, const drive_number residing = drive_c);
            bool installed(const uid pkg_uid);

            void migrate_legacy_registries();
            void load_registries();

            const std::size_t package_count() const {
                return objects_.size();
            }

            object_map_type::iterator begin() {
                return objects_.begin();
            }

            object_map_type::iterator end() {
                return objects_.end();
            }

            // No thread safe
            package::object *package(const uid app_uid, const std::int32_t index = 0);
            std::vector<package::object *> augmentations(const uid app_uid);

            bool add_package(package::object &pkg, const controller_info *controller_info);
            bool save_package(package::object &pkg);
            bool uninstall_package(package::object &pkg);

            bool install_package(const std::u16string &path, const drive_number drive, std::atomic<int> &progress, const bool silent = false);
        };
    }
}
