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
#include <common/log.h>
#include <common/path.h>

#include <core/loader/sis.h>
#include <core/loader/sis_script_interpreter.h>
#include <core/manager/package_manager.h>
#include <core/vfs.h>

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eka2l1 {
    namespace manager {
        bool package_manager::load_sdb_yaml(const std::string &path) {
            FILE *f = fopen(path.c_str(), "rb");

            if (!f) {
                return false;
            }

            YAML::Node sdb_node = YAML::LoadFile(path);

            for (const auto &maybe_app : sdb_node) {
                const YAML::Node app = maybe_app.second;

                app_info info;

                info.drive = app["drive"].as<uint8_t>();
                info.ver = static_cast<epocver>(app["epoc"].as<int>());

                auto exec = app["exec"].as<std::string>();
                info.executable_name = common::utf8_to_ucs2(exec);

                info.id = app["uid"].as<uint32_t>();

                auto name = app["name"].as<std::string>();
                info.name = common::utf8_to_ucs2(name);

                auto vendor = app["vendor"].as<std::string>();
                info.vendor_name = common::utf8_to_ucs2(vendor);

                if (info.drive == 0) {
                    c_apps.emplace(info.id, info);
                } else {
                    e_apps.emplace(info.id, info);
                }
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

            for (const auto &c_app : c_apps) {
                emitter << YAML::Key << common::ucs2_to_utf8(c_app.second.name);
                emitter << YAML::Value << YAML::BeginMap;

                emitter << YAML::Key << "drive" << YAML::Value << c_app.second.drive;
                emitter << YAML::Key << "epoc" << YAML::Value << static_cast<int>(c_app.second.ver);
                emitter << YAML::Key << "exec" << YAML::Value << common::ucs2_to_utf8(c_app.second.executable_name);
                emitter << YAML::Key << "uid" << YAML::Value << c_app.second.id;
                emitter << YAML::Key << "name" << YAML::Value << common::ucs2_to_utf8(c_app.second.name);
                emitter << YAML::Key << "vendor" << YAML::Value << common::ucs2_to_utf8(c_app.second.vendor_name);

                emitter << YAML::EndMap;
            }

            emitter << YAML::EndMap;

            out << emitter.c_str();

            return true;
        }

        bool package_manager::installed(uid app_uid) {
            auto res1 = c_apps.find(app_uid);
            auto res2 = e_apps.find(app_uid);

            return (res1 != c_apps.end()) || (res2 != e_apps.end());
        }

        std::u16string package_manager::app_name(uid app_uid) {
            auto res1 = c_apps.find(app_uid);
            auto res2 = e_apps.find(app_uid);

            if ((res1 == c_apps.end()) && (res2 == e_apps.end())) {
                LOG_WARN("App not installed on both drive, return null name.");
                return u"";
            }

            if (res1 != c_apps.end()) {
                return res1->second.name;
            }

            return res2->second.name;
        }

        bool package_manager::install_controller(loader::sis_controller *ctrl, uint8_t drv) {
            app_info info{};

            info.vendor_name = ctrl->info.vendor_name.unicode_string;
            info.name = ((loader::sis_string *)(ctrl->info.names.fields[0].get()))->unicode_string;
            info.drive = drv;
            info.executable_name = u"";
            info.id = ctrl->info.uid.uid;
            info.ver = epocver::epoc9;

            uid ruid = ctrl->info.uid.uid;

            LOG_INFO("Package UID: 0x{:x}", ruid);

            for (auto &wrap_file_des : ctrl->install_block.files.fields) {
                loader::sis_file_des *file_des = (loader::sis_file_des *)(wrap_file_des.get());
                bool is_exe = false;

                if (file_des->target.unicode_string.length() > 4) {
                    is_exe = file_des->target.unicode_string.substr(file_des->target.unicode_string.length() - 4) == u".exe";
                }

                if ((file_des->caps.raw_data.size() != 0) || is_exe) {
                    info.executable_name = file_des->target.unicode_string;

                    // Fixed drive
                    if (info.executable_name.find(u"!") == std::u16string::npos) {
                        std::u16string fixed_drive = info.executable_name.substr(0, 2);

                        if (fixed_drive == u"C:" || fixed_drive == u"c:") {
                            LOG_INFO("Fixed drive, unexpected change to C:");
                            info.drive = 0;
                        } else {
                            LOG_INFO("Fixed drive, unexpected change to E:");
                            info.drive = 1;
                        }

                        symfile f = io->open_file(file_des->target.unicode_string, READ_MODE | BIN_MODE);
                        f->seek(8, file_seek_mode::beg);
                        f->read_file(&info.id, 1, 4);
                        f->close();
                    }

                    LOG_INFO("Executable_name: {}", common::ucs2_to_utf8(info.executable_name));

                    if (info.drive == 0) {
                        info.executable_name[0] = u'C';
                    } else {
                        info.executable_name[0] = u'E';
                    }

                    symfile f = io->open_file(info.executable_name, READ_MODE | BIN_MODE);
                    f->seek(8, file_seek_mode::beg);
                    f->read_file(&info.id, 1, 4);
                    f->close();
                
                    // Get file name
                    size_t slash_pos = info.executable_name.find_last_of(u"\\");

                    if (slash_pos != std::u16string::npos) {
                        info.executable_name = info.executable_name.substr(slash_pos + 1);
                    }
                }
            }

            if (info.executable_name != u"") {
                if (info.drive == 0) {
                    c_apps.insert(std::make_pair(ruid, info));
                    c_apps[ruid] = info;
                } else {
                    e_apps.insert(std::make_pair(ruid, info));
                    e_apps[ruid] = info;
                }
            }

            for (auto &wrap_mini_ctrl : ctrl->install_block.controllers.fields) {
                loader::sis_controller *mini_ctrl = (loader::sis_controller *)(wrap_mini_ctrl.get());
                // Recursively install controller
                install_controller(mini_ctrl, drv);
            }

            return true;
        }

        bool package_manager::install_package(const std::u16string &path, uint8_t drive) {
            std::optional<epocver> sis_ver = loader::get_epoc_ver(common::ucs2_to_utf8(path));

            if (!sis_ver) {
                return false;
            }

            if (*sis_ver == epocver::epoc9) {
                loader::sis_contents res = loader::parse_sis(common::ucs2_to_utf8(path));

                // Interpret the file
                loader::ss_interpreter interpreter(std::make_shared<std::ifstream>(common::ucs2_to_utf8(path), std::ios::binary),
                    io,
                    this,
                    res.controller.install_block,
                    res.data,
                    loader::sis_drive(drive));

                interpreter.interpret();

                install_controller(&res.controller, drive);
            } else {
                app_info de_info;
                loader::install_sis_old(path, io, drive, *sis_ver, de_info);

                if (de_info.drive == 0) {
                    c_apps.emplace(de_info.id, de_info);
                } else {
                    e_apps.emplace(de_info.id, de_info);
                }
            }

            write_sdb_yaml("apps_registry.yml");

            return true;
        }

        bool package_manager::uninstall_package(uid app_uid) {
            c_apps.erase(app_uid);
            e_apps.erase(app_uid);

            write_sdb_yaml("apps_registry.yml");

            return false;
        }

        // Package manager
        app_info package_manager::info(uid app_uid) {
            auto res1 = c_apps.find(app_uid);
            auto res2 = e_apps.find(app_uid);

            if (res1 == c_apps.end() && res2 == e_apps.end()) {
                LOG_WARN("Cant find info of app with UID of: {}", app_uid);
                return app_info{};
            }

            if (res1 != c_apps.end()) {
                return res1->second;
            }

            return res2->second;
        }

        std::string package_manager::get_app_executable_path(uint32_t uid) {
            app_info inf = info(uid);

            std::string res = (inf.drive == 0) ? "C:" : "E:";
            res += "/sys/bin/";
            res += common::ucs2_to_utf8(inf.executable_name);

            return res;
        }

        std::string package_manager::get_app_name(uint32_t uid) {
            app_info inf = info(uid);
            return common::ucs2_to_utf8(inf.name);
        }
    }
}
