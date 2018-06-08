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
#include <common/log.h>
#include <common/path.h>
#include <core/vfs.h>

#include <iostream>

#include <map>
#include <mutex>
#include <thread>

namespace eka2l1 {
    namespace vfs {

        std::string _pref_path;
        std::string _current_dir;

        std::map<std::string, std::string> _mount_map;

        std::mutex _gl_mut;

        void init() {
            _current_dir = "C:";

            mount("C:", "/home/dtt2502/EKA2L1/partitions/C");
            mount("E:", "/home/dtt2502/EKA2L1/partitions/E");
        }

        void shutdown() {
            unmount("C:");
            unmount("E:");
        }

        std::string current_dir() {
            return _current_dir;
        }

        void current_dir(const std::string &new_dir) {
            std::lock_guard<std::mutex> guard(_gl_mut);
            _current_dir = new_dir;
        }

        void mount(const std::string &dvc, const std::string &real_path) {
            auto find_res = _mount_map.find(dvc);

            if (find_res == _mount_map.end()) {
                // Warn
            }

            std::lock_guard<std::mutex> guard(_gl_mut);
            _mount_map.insert(std::make_pair(dvc, real_path));

            _current_dir = dvc;
        }

        void unmount(const std::string &dvc) {
            std::lock_guard<std::mutex> guard(_gl_mut);
            _mount_map.erase(dvc);
        }

        std::string get(std::string vir_path) {
            std::string abs_path = "";

            std::string current_dir = _current_dir;

            // Current directory is always an absolute path
            std::string partition;

            if (vir_path.find_first_of(':') == 1) {
                partition = vir_path.substr(0, 2);
            } else {
                partition = current_dir.substr(0, 2);
            }

            partition[0] = std::toupper(partition[0]);

            auto res = _mount_map.find(partition);

            if (res == _mount_map.end()) {
                //log_error("Could not find partition");
                return "";
            }

            current_dir = res->second + _current_dir.substr(2);

            // Make it case-insensitive
            for (auto &c : vir_path) {
                c = std::tolower(c);
            }

            if (!is_absolute(vir_path, current_dir)) {
                abs_path = absolute_path(vir_path, current_dir);
            } else {
                abs_path = add_path(res->second, vir_path.substr(2));
            }

            return abs_path;
        }
    }
}

