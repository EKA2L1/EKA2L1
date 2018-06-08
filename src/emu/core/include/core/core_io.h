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

#include <map>
#include <mutex>
#include <string>

namespace eka2l1 {
    namespace loader {
        class rom;
    }

    // represents a file
    struct file {
    };

    class io_system {
        std::string _pref_path;
        std::string _current_dir;

        std::map<std::string, std::string> _mount_map;

        std::mutex _gl_mut;

    public:
        void init();
        void shutdown();

        std::string current_dir();
        void current_dir(const std::string &new_dir);

        // Case 1: Mount a real directory
        void mount(const std::string &dvc, const std::string &real_path);

        // Case 2: Virtual mount: This case, mount the ROM for e.g
        void mount(const std::string &dvc, loader::rom *rom_des);

        std::shared_ptr<file> read_file(const std::string &path) {
            return std::make_shared<file>();
        }

        void unmount(const std::string &dvc);

        // Map a virtual path to real path
        std::string get(std::string vir_path);
    }
}

