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
#include <string>

// Symbian separators is \\
// This is mostly same as Windows

namespace eka2l1 {
    bool is_absolute(std::string str, std::string current_dir, bool symbian_use = false);
    std::string absolute_path(std::string str, std::string current_dir, bool symbian_use = false);

    bool is_relative(std::string str, bool symbian_use = false);
    std::string relative_path(std::string str, bool symbian_use = false);

    std::string add_path(const std::string &path1, const std::string &path2, bool symbian_use = false);

    bool has_root_name(std::string path, bool symbian_use = false);
    std::string root_name(std::string path, bool symbian_use = false);

    bool has_root_dir(std::string path, bool symbian_use = false);
    std::string root_dir(std::string path, bool symbian_use = false);

    bool has_root_path(std::string path, bool symbian_use = false);
    std::string root_path(std::string path, bool symbian_use = false);

    bool has_filename(std::string path, bool symbian_use = false);
    std::string filename(std::string path, bool symbian_use = false);

    std::string file_directory(std::string path, bool symbian_use = false);

    // Since I'm too desperate
    struct path_iterator {
        std::string path;
        std::string comp;
        uint32_t crr_pos;

    public:
        path_iterator()
            : crr_pos(0) {
            ++(*this);
        }

        path_iterator(std::string p)
            : path(p)
            , crr_pos(0) {
            ++(*this);
        }

        void operator++() {
            if (crr_pos < path.length())
                comp = "";

            while ((crr_pos < path.length()) && (path[crr_pos] != '/') && (path[crr_pos] != '\\')) {
                comp += path[crr_pos];
                crr_pos += 1;
            }

            crr_pos += 1;
        }

        std::string operator*() const {
            return comp;
        }

        operator bool() const {
            return path.length() >= crr_pos - 1;
        }
    };

    void create_directory(std::string path);
    bool exists(std::string path);
    bool is_dir(std::string path);
    void create_directories(std::string path);
}

