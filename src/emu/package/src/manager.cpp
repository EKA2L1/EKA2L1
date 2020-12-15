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

#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <loader/sis.h>

#include <config/config.h>

#include <package/manager.h>
#include <package/sis_script_interpreter.h>
#include <package/sis_v1_installer.h>
#include <vfs/vfs.h>

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    namespace manager {
        static constexpr const char *APP_REGISTRY_FILENAME = "apps_registry.yml";
        static constexpr const char *PACKAGE_FOLDER_PATH = "packages";

        packages::packages(io_system *io, config::state *conf)
            : sys(io)
            , conf(conf) {
            load_sdb_yaml(add_path(conf->storage, APP_REGISTRY_FILENAME));
            eka2l1::create_directory(add_path(conf->storage, PACKAGE_FOLDER_PATH));
        }

        static std::string get_bucket_stream_path(config::state *state, const uid package_uid) {
            return add_path(state->storage, add_path(PACKAGE_FOLDER_PATH, common::to_string(package_uid, std::hex) + ".txt"));
        }

        bool packages::add_to_file_bucket(const uid package_uid, const std::string &path) {
            std::fstream bucket_stream(get_bucket_stream_path(conf, package_uid), std::ios::out | std::ios::app);

            if (!bucket_stream) {
                return false;
            }

            bucket_stream << path << '\n';
            return true;
        }

        void packages::get_file_bucket(const manager::uid pkg_uid, std::vector<std::string> &paths) {
            std::fstream bucket_stream(get_bucket_stream_path(conf, pkg_uid), std::ios::in);

            if (!bucket_stream) {
                return;
            }

            std::string path = "";

            while (std::getline(bucket_stream, path)) {
                paths.push_back(path);
            }
        }

        bool packages::load_sdb_yaml(const std::string &path) {
            FILE *f = fopen(path.c_str(), "rb");

            if (!f) {
                return false;
            }

            YAML::Node sdb_node = YAML::LoadFile(path);

            for (const auto &maybe_app : sdb_node) {
                const YAML::Node app = maybe_app.second;

                package_info info;

                info.drive = static_cast<drive_number>(app["drive"].as<int>());
                info.id = app["uid"].as<manager::uid>();

                const auto name = app["name"].as<std::string>();
                info.name = common::utf8_to_ucs2(name);

                const auto vendor = app["vendor"].as<std::string>();
                info.vendor_name = common::utf8_to_ucs2(vendor);

                add_package(info);
            }

            return true;
        }

        bool packages::write_sdb_yaml(const std::string &path) {
            YAML::Emitter emitter;

            std::ofstream out(path);

            if (out.fail()) {
                return false;
            }

            emitter << YAML::BeginMap;

            for (const auto &package : pkgs) {
                emitter << YAML::Key << common::ucs2_to_utf8(package.name);
                emitter << YAML::Value << YAML::BeginMap;

                emitter << YAML::Key << "drive" << YAML::Value << package.drive;
                emitter << YAML::Key << "uid" << YAML::Value << package.id;
                emitter << YAML::Key << "name" << YAML::Value << common::ucs2_to_utf8(package.name);
                emitter << YAML::Key << "vendor" << YAML::Value << common::ucs2_to_utf8(package.vendor_name);

                emitter << YAML::EndMap;
            }

            emitter << YAML::EndMap;

            out << emitter.c_str();

            return true;
        }

        const package_info *packages::package(const std::size_t idx) const {
            if (pkgs.size() <= idx) {
                return nullptr;
            }

            return &pkgs[idx];
        }

        void packages::add_package(package_info &pkg) {
            // Lock the list.
            const std::lock_guard<std::mutex> guard(lockdown);

            // Add to package list
            pkgs.push_back(std::move(pkg));
            std::sort(pkgs.begin(), pkgs.end(), [](const package_info &lhs, const package_info &rhs) {
                return lhs.id < rhs.id;
            });
        }

        bool packages::installed(uid app_uid) {
            // Lock the list.
            const std::lock_guard<std::mutex> guard(lockdown);

            package_info temp;
            temp.id = app_uid;

            return std::binary_search(pkgs.begin(), pkgs.end(), temp,
                [](const package_info &lhs, const package_info &rhs) {
                    return lhs.id < rhs.id;
                });
        }

        bool packages::install_controller(loader::sis_controller *ctrl, const drive_number drive) {
            package_info info{};

            info.vendor_name = ctrl->info.vendor_name.unicode_string;
            info.name = ((loader::sis_string *)(ctrl->info.names.fields[0].get()))->unicode_string;
            info.drive = drive;
            info.id = ctrl->info.uid.uid;

            if (installed(info.id)) {
                return false;
            }

            LOG_INFO(PACKAGE, "Package UID: 0x{:x}", info.id);

            for (auto &wrap_file_des : ctrl->install_block.files.fields) {
                const loader::sis_file_des *file_des = reinterpret_cast<loader::sis_file_des *>(wrap_file_des.get());

                std::string file_path = common::ucs2_to_utf8(file_des->target.unicode_string);

                if (file_path[0] == '!') {
                    file_path[0] = static_cast<char>(drive_to_char16(drive));
                }

                // If we are really going to install this
                if (file_des->op == loader::ss_op::EOpInstall) {
                }
            }

            add_package(info);

            for (auto &wrap_mini_ctrl : ctrl->install_block.controllers.fields) {
                loader::sis_controller *mini_ctrl = (loader::sis_controller *)(wrap_mini_ctrl.get());
                // Recursively install controller
                install_controller(mini_ctrl, drive);
            }

            return true;
        }

        bool packages::install_package(const std::u16string &path, const drive_number drive,
            std::atomic<int> &progress) {
            std::optional<epocver> sis_ver = loader::get_epoc_ver(common::ucs2_to_utf8(path));

            if (!sis_ver) {
                return false;
            }

            if (*sis_ver == epocver::epoc94) {
                loader::sis_contents res = loader::parse_sis(common::ucs2_to_utf8(path));
                common::ro_std_file_stream stream(common::ucs2_to_utf8(path), true);

                // Interpret the file
                loader::ss_interpreter interpreter(reinterpret_cast<common::ro_stream *>(&stream),
                    sys, this, &res.controller, &res.data, drive);

                // Set up hooks
                if (show_text) {
                    interpreter.show_text = show_text;
                }

                if (choose_lang) {
                    interpreter.choose_lang = choose_lang;
                }

                if (var_resolver) {
                    interpreter.var_resolver = var_resolver;
                }

                interpreter.interpret(progress);
                install_controller(&res.controller, drive);
            } else {
                package_info de_info;
                std::vector<std::u16string> files;

                loader::install_sis_old(path, sys, drive, de_info, files);

                if (installed(de_info.id)) {
                    return false;
                }

                for (const auto &file : files) {
                    add_to_file_bucket(de_info.id, common::ucs2_to_utf8(file));
                }

                add_package(de_info);
            }

            write_sdb_yaml(add_path(conf->storage, APP_REGISTRY_FILENAME));

            if (show_text) {
                show_text("Installation done!", true);
            }

            LOG_TRACE(PACKAGE, "Installation done!");

            return true;
        }

        void packages::delete_files_and_bucket(const uid pkg_uid) {
            // Get the package file, delete all related files
            const std::string pkg_file = get_bucket_stream_path(conf, pkg_uid);
            std::ifstream bucket_stream(pkg_file);

            if (bucket_stream) {
                std::string file_path = "";

                while (std::getline(bucket_stream, file_path)) {
                    sys->delete_entry(common::utf8_to_ucs2(file_path));
                }

                // Remove myself too!
                bucket_stream.close();
                common::remove(pkg_file);
            }
        }

        bool packages::uninstall_package(const uid pkg_uid) {
            auto pkg = std::lower_bound(pkgs.begin(), pkgs.end(), pkg_uid,
                [](const package_info &lhs, const manager::uid auid) {
                    return lhs.id < auid;
                });

            if (pkg == pkgs.end()) {
                return false;
            }

            pkgs.erase(pkg);

            std::sort(pkgs.begin(), pkgs.end(), [](const package_info &lhs, const package_info &rhs) {
                return lhs.id < rhs.id;
            });

            delete_files_and_bucket(pkg_uid);

            write_sdb_yaml(add_path(conf->storage, APP_REGISTRY_FILENAME));

            return true;
        }
    }
}
