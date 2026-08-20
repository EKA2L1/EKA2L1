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
#include <package/common.h>

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

    namespace package {
        enum installation_result {
            installation_result_success = 0,
            installation_result_aborted = 1,
            installation_result_invalid = 2
        };
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

            // Delete "<drive>:\private\<sid>\" on every writable drive: the data
            // directory that belongs to an executable which has just been removed.
            void remove_private_directories(const epoc::uid sid);

            // Delete the files an installed package owns that its replacement does
            // not, so an upgrade stops dragging the old version's leftovers along.
            void remove_stale_files(package::object &installed, const package::object &replacement);

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
            package::object *package(const uid app_uid, const std::u16string package_name, const std::u16string vendor_name);
            std::vector<package::object *> augmentations(const uid app_uid);
            std::vector<package::object *> dependents(const uid app_uid);
            std::vector<uid> installed_uids() const;

            bool controller(const uid app_uid, const std::uint32_t package_index, const std::uint32_t controller_offset,
                loader::sis_controller &controller_output);

            bool add_package(package::object &pkg, const controller_info *controller_info);
            bool save_package(package::object &pkg);
            /**
             * \brief Find the package that installed an executable, by its secure ID.
             * \returns Null when no installed package claims it.
             */
            package::object *package_owning_executable(const uid secure_id);

            /**
             * \brief Find the package that installed a file, by its path.
             *
             * Falls back to matching drive and file name when the exact path is not
             * claimed, and then only answers if exactly one package matches.
             *
             * \returns Null when no installed package claims it.
             */
            package::object *package_owning_file(const std::u16string &file_path);

            bool uninstall_package(package::object &pkg);
            bool remove_registeration(package::object &pkg);

            /**
             * \brief Install a package.
             *
             * \param silent    Do not ask the user anything; pick defaults instead.
             * \param as_stub   The package is a stub describing software already in
             *                   the ROM, so it must not be reported as removable.
             */
            package::installation_result install_package(const std::u16string &path, const drive_number drive, progress_changed_callback progress_cb = nullptr,
                cancel_requested_callback cancel_cb = nullptr, const bool silent = false, const bool as_stub = false);
        };
    }
}
