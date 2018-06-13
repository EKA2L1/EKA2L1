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
#include <loader/sis.h>
#include <loader/sis_script_interpreter.h>
#include <manager/package_manager.h>
#include <vfs.h>

#include <fstream>

namespace eka2l1 {
    namespace manager {

        struct sdb_header {
            char magic[4];
            uint32_t app_count;
            uint64_t uid_offset;
            uint64_t name_offset;
            uint64_t drive_offset;
            uint64_t vendor_offset;
            uint64_t ename_offset;
        };

        bool package_manager::write_sdb(const std::string &path) {
            FILE *file = fopen(path.c_str(), "wb");

            if (!file) {
                return false;
            }

            int total_app = c_apps.size() + e_apps.size();

            uint64_t uid_offset = 48;
            uint64_t name_offset = uid_offset + total_app * 4;
            uint64_t drive_offset = name_offset + total_app * 4;

            for (auto &c_app : c_apps) {
                drive_offset += c_app.second.name.length() * 2;
            }

            for (auto &e_app : e_apps) {
                drive_offset += e_app.second.name.length() * 2;
            }

            uint64_t vendor_offset = drive_offset + total_app;
            uint64_t ename_offset = vendor_offset + total_app * 4;

            for (auto &c_app : c_apps) {
                ename_offset += c_app.second.vendor_name.size() * 2;
            }

            for (auto &e_app : e_apps) {
                ename_offset += e_app.second.vendor_name.size() * 2;
            }

            fwrite("sdbf", 1, 4, file);
            fwrite(&total_app, 1, 4, file);
            fwrite(&uid_offset, 1, 8, file);
            fwrite(&name_offset, 1, 8, file);
            fwrite(&drive_offset, 1, 8, file);
            fwrite(&vendor_offset, 1, 8, file);
            fwrite(&ename_offset, 1, 8, file);

            for (auto &c_app : c_apps) {
                fwrite(&c_app.first, 1, 4, file);
            }

            for (auto &e_app : e_apps) {
                fwrite(&e_app.first, 1, 4, file);
            }

            for (auto &c_app : c_apps) {
                int size = c_app.second.name.size();

                fwrite(&size, 1, 4, file);
                fwrite(&c_app.second.name[0], 2, c_app.second.name.size(), file);
            }

            for (auto &e_app : e_apps) {
                int size = e_app.second.name.size();

                fwrite(&size, 1, 4, file);
                fwrite(&e_app.second.name[0], 2, e_app.second.name.size(), file);
            }

            for (auto &c_app : c_apps) {
                fwrite(&c_app.second.drive, 1, 1, file);
            }

            for (auto &e_app : e_apps) {
                fwrite(&e_app.second.drive, 1, 4, file);
            }

            for (auto &c_app : c_apps) {
                int size = c_app.second.name.size();

                fwrite(&size, 1, 4, file);
                fwrite(c_app.second.vendor_name.data(), 2, c_app.second.vendor_name.size(), file);
            }

            for (auto &e_app : e_apps) {
                int size = e_app.second.name.size();

                fwrite(&size, 1, 4, file);
                fwrite(&e_app.second.vendor_name[0], 2, e_app.second.vendor_name.size(), file);
            }

            for (auto &c_app : c_apps) {
                int size = c_app.second.executable_name.size();

                fwrite(&size, 1, 4, file);
                fwrite(&c_app.second.executable_name[0], 2, c_app.second.executable_name.size(), file);
            }

            for (auto &e_app : e_apps) {
                int size = e_app.second.executable_name.size();

                fwrite(&size, 1, 4, file);
                fwrite(&e_app.second.executable_name[0], 2, e_app.second.executable_name.size(), file);
            }

            fclose(file);

            return true;
        }

        // Load registry contains all installed apps
        // This is a quick format I designed when I was on my way
        // going home :P
        bool package_manager::load_sdb(const std::string &path) {
            FILE *file = fopen(path.c_str(), "rb");

            if (!file) {
                return false;
            }

            sdb_header header;

            fread(header.magic, 1, 4, file);

            if (strncmp(header.magic, "sdbf", 4) != 0) {
                return false;
            }

            fread(&header.app_count, 1, 4, file);
            fread(&header.uid_offset, 1, 8, file);
            fread(&header.name_offset, 1, 8, file);
            fread(&header.drive_offset, 1, 8, file);
            fread(&header.vendor_offset, 1, 8, file);
            fread(&header.ename_offset, 1, 8, file);

            std::vector<uid> uids(header.app_count);

            fseek(file, header.uid_offset, SEEK_SET);

            for (uint32_t i = 0; i < header.app_count; i++) {
                fread(&uids[i], 1, 4, file);
            }

            fseek(file, header.name_offset, SEEK_SET);

            std::vector<std::u16string> names(header.app_count);

            for (uint32_t i = 0; i < header.app_count; i++) {
                int name_len = 0;

                fread(&name_len, 1, 4, file);
                names[i].resize(name_len);

                fread(&(names[i])[0], 2, name_len, file);
            }

            std::vector<uint8_t> drives(header.app_count);

            fseek(file, header.drive_offset, SEEK_SET);

            for (uint32_t i = 0; i < header.app_count; i++) {
                fread(&drives[i], 1, 1, file);
            }

            fseek(file, header.vendor_offset, SEEK_SET);

            std::vector<std::u16string> vendor_names(header.app_count);

            for (uint32_t i = 0; i < header.app_count; i++) {
                int name_len = 0;

                fread(&name_len, 1, 4, file);
                vendor_names[i].resize(name_len);

                fread(&(vendor_names[i])[0], 2, name_len, file);
            }

            fseek(file, header.ename_offset, SEEK_SET);

            std::vector<std::u16string> exe_names(header.app_count);

            for (uint32_t i = 0; i < header.app_count; i++) {
                int name_len = 0;

                fread(&name_len, 1, 4, file);
                exe_names[i].resize(name_len);

                fread(&(exe_names[i])[0], 2, name_len, file);
            }

            // Load those into the manager
            for (uint32_t i = 0; i < header.app_count; i++) {
                app_info info{};

                info.drive = drives[i];
                info.name = names[i];
                info.vendor_name = vendor_names[i];
                info.executable_name = exe_names[i].data();
                info.id = uids[i];

                // Drive C
                if (drives[i] == 0) {
                    c_apps.insert(std::make_pair(uids[i], info));
                } else {
                    e_apps.insert(std::make_pair(uids[i], info));
                }
            }

            fclose(file);

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
                    }

                    LOG_INFO("Executable_name: {}", std::string(info.executable_name.begin(), info.executable_name.end()));
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

                install_controller(&res.controller, drive);

                // Interpret the file
                loader::ss_interpreter interpreter(std::make_shared<std::ifstream>(common::ucs2_to_utf8(path), std::ios::binary),
                    io,
                    this,
                    res.controller.install_block,
                    res.data,
                    loader::sis_drive(drive));

                interpreter.interpret();
            } else {
                loader::sis_old res = *loader::parse_sis_old(common::ucs2_to_utf8(path));
                std::u16string main_path = res.app_path ? *res.app_path : (res.exe_path ? *res.exe_path : u"");

                if (main_path != u"") {
                    app_info info;

                    if (main_path.find(u"!") == std::u16string::npos) {
                        if (main_path.find(u"C:") != std::u16string::npos || main_path.find(u"c:") != std::u16string::npos) {
                            info.drive = 0;
                        } else {
                            info.drive = 1;
                        }
                    } else {
                        info.drive = drive;
                    }

                    size_t last_slash_pos = main_path.find_last_of(u"\\");
                    size_t sub_pos = res.app_path ? main_path.find(u".app") : main_path.find(u".exe");

                    std::u16string name = (last_slash_pos != std::u16string::npos) ? main_path.substr(last_slash_pos + 1, sub_pos - last_slash_pos - 1) : u"Unknown";

                    info.executable_name = main_path;
                    info.id = res.header.uid1;
                    info.vendor_name = u"Your mom";
                    info.name = name;
                    info.executable_name = main_path.substr(last_slash_pos + 1);
                    info.ver = *sis_ver;

                    if (info.drive == 0) {
                        c_apps.emplace(info.id, info);
                    } else {
                        e_apps.emplace(info.id, info);
                    }

                    for (auto &file : res.files) {
                        std::u16string dest = file.dest;

                        if (dest.find(u"!") != std::u16string::npos) {
                            dest.replace(0, 1, (info.drive == 0) ? u"c" : u"e");
                        }

                        std::string rp = eka2l1::file_directory(io->get(common::ucs2_to_utf8(dest)));
                        eka2l1::create_directories(rp);

                        symfile f = io->open_file(dest, WRITE_MODE | BIN_MODE);

                        size_t left = file.record.len;
                        size_t chunk = 0x2000;

                        std::vector<char> temp;
                        temp.resize(chunk);

                        FILE *sis_file = fopen(common::ucs2_to_utf8(path).data(), "rb");
                        fseek(sis_file, file.record.ptr, SEEK_SET);

                        while (left > 0) {
                            size_t took = left < chunk ? left : chunk;
                            size_t readed = fread(temp.data(), 1, took, sis_file);

                            if (readed != took) {
                                fclose(sis_file);
                                f->close();

                                return false;
                            }

                            f->write_file(temp.data(), 1, took);

                            left -= took;
                        }
                    }
                }
            }

            write_sdb("apps_registry.sdb");

            return true;
        }

        bool package_manager::uninstall_package(uid app_uid) {
            c_apps.erase(app_uid);
            e_apps.erase(app_uid);

            write_sdb("apps_registry.sdb");

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
