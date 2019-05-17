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

#include <manager/sis.h>

#include <manager/package_manager.h>
#include <manager/sis_script_interpreter.h>
#include <manager/sis_v1_installer.h>

#include <epoc/vfs.h>

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    namespace manager {
        package_manager::package_manager(io_system *io)
            : io(io) 
        { 
            load_sdb_yaml("apps_registry.yml");
            eka2l1::create_directory("packages");
        }
            
        bool package_manager::add_to_file_bucket(const uid package_uid, const std::string &path) {
            std::fstream bucket_stream("packages\\" + common::to_string(package_uid, std::hex) + ".txt",
                std::ios::out | std::ios::app);

            if (!bucket_stream) {
                return false;
            }

            bucket_stream << path << '\n';
            return true;
        }
        
        bool package_manager::load_sdb_yaml(const std::string &path) {
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

                packages.emplace(info.id, info);
            }

            return true;
        }

        bool package_manager::write_sdb_yaml(const std::string &path) {
            YAML::Emitter emitter;

            std::ofstream out(path);

            if (out.fail()) {
                return false;
            }

            emitter << YAML::BeginMap;

            for (const auto &package : packages) {
                emitter << YAML::Key << common::ucs2_to_utf8(package.second.name);
                emitter << YAML::Value << YAML::BeginMap;

                emitter << YAML::Key << "drive" << YAML::Value << package.second.drive;
                emitter << YAML::Key << "uid" << YAML::Value << package.second.id;
                emitter << YAML::Key << "name" << YAML::Value << common::ucs2_to_utf8(package.second.name);
                emitter << YAML::Key << "vendor" << YAML::Value << common::ucs2_to_utf8(package.second.vendor_name);

                emitter << YAML::EndMap;
            }

            emitter << YAML::EndMap;

            out << emitter.c_str();

            return true;
        }

        bool package_manager::installed(uid app_uid) {
            return (packages.find(app_uid) != packages.end());
        }

        bool package_manager::install_controller(loader::sis_controller *ctrl, drive_number drv) {
            package_info info{};

            info.vendor_name = ctrl->info.vendor_name.unicode_string;
            info.name = ((loader::sis_string *)(ctrl->info.names.fields[0].get()))->unicode_string;
            info.drive = drv;
            info.id = ctrl->info.uid.uid;

            LOG_INFO("Package UID: 0x{:x}", info.id);

            for (auto &wrap_file_des : ctrl->install_block.files.fields) {
                const loader::sis_file_des *file_des = reinterpret_cast<loader::sis_file_des *>(wrap_file_des.get());
                
                std::string file_path = common::ucs2_to_utf8(file_des->target.unicode_string);

                if (file_path[0] == '!') {
                    file_path[0] = static_cast<char>(drive_to_char16(drv));
                }

                // If we are really going to install this
                if (file_des->op == loader::ss_op::EOpInstall) {
                    add_to_file_bucket(info.id, file_path);
                }
            }

            // Add to package list
            packages[info.id] = std::move(info);

            for (auto &wrap_mini_ctrl : ctrl->install_block.controllers.fields) {
                loader::sis_controller *mini_ctrl = (loader::sis_controller *)(wrap_mini_ctrl.get());
                // Recursively install controller
                install_controller(mini_ctrl, drv);
            }

            return true;
        }

        bool package_manager::install_package(const std::u16string &path, drive_number drive) {
            std::optional<epocver> sis_ver = loader::get_epoc_ver(common::ucs2_to_utf8(path));

            if (!sis_ver) {
                return false;
            }

            if (*sis_ver == epocver::epoc94) {
                loader::sis_contents res = loader::parse_sis(common::ucs2_to_utf8(path));

                // Interpret the file
                loader::ss_interpreter interpreter(std::make_shared<std::ifstream>(common::ucs2_to_utf8(path), std::ios::binary),
                    io,
                    this,
                    res.controller.install_block,
                    res.data,
                    drive);

                interpreter.interpret();

                install_controller(&res.controller, drive);
            } else {
                package_info de_info;
                std::vector<std::u16string> files;

                loader::install_sis_old(path, io, drive, de_info, files);

                for (const auto &file: files) {
                    add_to_file_bucket(de_info.id, common::ucs2_to_utf8(file));
                }

                packages.emplace(de_info.id, de_info);
            }

            write_sdb_yaml("apps_registry.yml");

            return true;
        }

        bool package_manager::uninstall_package(const uid pkg_uid) {
            packages.erase(pkg_uid);

            const std::string pkg_file = "packages\\" + common::to_string(pkg_uid, std::hex) + ".txt";

            // Get the package file, delete all related files
            std::fstream bucket_stream(pkg_file, std::ios::in);

            if (!bucket_stream) {
                return false;
            }

            std::string file_path = "";

            while (std::getline(bucket_stream, file_path)) {
                io->delete_entry(common::utf8_to_ucs2(file_path));
            }

            // Remove myself too!
            common::remove(pkg_file);
            write_sdb_yaml("apps_registry.yml");

            return true;
        }
    }
}
