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

#include <common/path.h>
#include <common/platform.h>

#include <cstring>
#include <iostream>

#include <common/log.h>

#if EKA2L1_PLATFORM(UNIX) || EKA2L1_PLATFORM(DARWIN)
#include <sys/stat.h>
#include <unistd.h>
#endif

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

namespace eka2l1 {
    char get_separator(bool symbian_use) {
        if (symbian_use) {
            return '\\';
        }

#if EKA2L1_PLATFORM(WIN32)
        return '\\';
#else
        return '/';
#endif
    }

    char16_t get_separator_16(bool symbian_use) {
        if (symbian_use) {
            return u'\\';
        }

#if EKA2L1_PLATFORM(WIN32)
        return u'\\';
#else
        return u'/';
#endif
    }

    bool is_separator(const char sep) {
        if (sep == '/' || sep == '\\') {
            return true;
        }

        return false;
    }

    bool is_separator(const char16_t sep) {
        return (sep == '/' || sep == '\\');
    }

    template <typename T>
    std::basic_string<T> path_extension_impl(const std::basic_string<T> &path) {
        std::size_t last_dot_pos = path.find_last_of(static_cast<T>('.'));

        if (last_dot_pos == std::string::npos) {
            return std::basic_string<T>{};
        }

        return path.substr(last_dot_pos, path.length() - last_dot_pos);
    }

    template <typename T>
    std::basic_string<T> replace_extension_impl(const std::basic_string<T> &path, const std::basic_string<T> &new_ext) {
        std::size_t last_dot_pos = path.find_last_of(static_cast<T>('.'));

        if (last_dot_pos == std::string::npos) {
            return path + new_ext;
        }

        return path.substr(0, last_dot_pos) + new_ext;
    }

    template <typename T>
    std::basic_string<T> transform_separators(std::basic_string<T> path,
        bool symbian_use, std::function<T(bool)> separator_func) {
        size_t crr_point = 0;
        T dsep = separator_func(symbian_use);

        while (crr_point < path.size()) {
            if (is_separator(path[crr_point])) {
                path[crr_point] = dsep;
            }

            crr_point += 1;
        }

        return path;
    }

    template <typename T>
    std::basic_string<T> add_path_impl(const std::basic_string<T> &path1, const std::basic_string<T> &path2,
        bool symbian_use, std::function<T(bool)> separator_func) {
        using generic_string = std::basic_string<T>;

        generic_string nstring;
        generic_string merge;

        if (path1.length() == 0 && path2.length() == 0) {
            return generic_string{};
        } else if (path1.length() == 0) {
            merge = path2;
        } else if (path2.length() == 0) {
            merge = path1;
        } else {
            bool end_sep = is_separator(path1[path1.size() - 1]);
            bool beg_sep = is_separator(path2[0]);

            if (end_sep && beg_sep) {
                auto pos_sub = path2.find_first_not_of(path2[0]);

                if (pos_sub == std::string::npos) {
                    return transform_separators(path1, symbian_use, separator_func);
                }

                nstring = path2.substr(pos_sub);
            } else if (!end_sep && !beg_sep) {
                nstring = generic_string{ separator_func(symbian_use) } + path2;
            } else {
                nstring = path2;
            }

            merge = path1 + nstring;
        }

        // Turn all slash into / (quick hack)
        return transform_separators(merge, symbian_use, separator_func);
    }

    template <typename T>
    std::basic_string<T> relative_path_impl(std::basic_string<T> str, bool symbian_use) {
        std::basic_string<T> root = root_path(str, symbian_use);
        return str.substr(root.size());
    }

    template <typename T>
    std::basic_string<T> absolute_path_impl(std::basic_string<T> str, std::basic_string<T> current_dir,
        bool symbian_use, std::function<T(bool)> separator_func) {
        using generic_string = std::basic_string<T>;

        bool root_dirb = has_root_dir(str, symbian_use);
        bool root_drive = has_root_name(str, symbian_use);

        // Absoluted
        if (root_dirb) {
            return str;
        }

        generic_string new_str;

        if (!root_dirb && !root_drive) {
            new_str = add_path(current_dir, str, symbian_use);

            return new_str;
        }

        if (!root_drive && root_dirb) {
            generic_string n_root_drive = root_name(current_dir, symbian_use);
            new_str = add_path(n_root_drive, str, symbian_use);

            return new_str;
        }

        if (root_drive && !root_dirb) {
            generic_string root_drive_p = root_name(str, symbian_use);
            generic_string root_dir_n = root_dir(current_dir, symbian_use);
            generic_string relative_path_n = relative_path_impl<T>(current_dir, symbian_use);
            generic_string relative_path_p = relative_path_impl<T>(str, symbian_use);

            new_str = add_path(root_drive_p, root_dir_n, symbian_use);
            new_str = add_path(new_str, relative_path_n, symbian_use);
            new_str = add_path(new_str, relative_path_p, symbian_use);

            return new_str;
        }

        return generic_string{};
    }

    template <typename T>
    std::basic_string<T> root_name_impl(std::basic_string<T> path, bool symbian_use,
        std::function<T(bool)> separator_func) {
        using generic_string = decltype(path);

        bool has_drive = (path.length() >= 2) && (path[1] == ':');
        bool has_net = is_separator(path[0]) && (path[0] == path[1]);

        if (has_drive) {
            return path.substr(0, 2);
        } else if (has_net) {
            auto res = path.find_first_of(separator_func(symbian_use), 2);

            if (res == generic_string::npos) {
                return generic_string{};
            }

            return path.substr(0, res);
        }

        return generic_string{};
    }

    template <typename T>
    std::basic_string<T> root_dir_impl(std::basic_string<T> path, bool symbian_use,
        std::function<T(bool)> separator_func) {
        using generic_string = decltype(path);

        bool has_drive = (path.length() >= 2) && (path[1] == ':');
        bool has_net = is_separator(path[0]) && (path[0] == path[1]) && !symbian_use;

        if (has_drive) {
            if (path.size() > 2 && is_separator(path[2])) {
                return path.substr(2, 1);
            }
        } else if (has_net) {
            auto res = path.find_first_of(get_separator(symbian_use), 2);

            if (res == generic_string::npos) {
                return generic_string{};
            }

            return path.substr(res, 1);
        } else {
            if (path.size() > 1 && is_separator(path[0]) && !symbian_use) {
                return path.substr(0, 1);
            }
        }

        return generic_string{};
    }

    template <typename T>
    std::basic_string<T> root_path_impl(std::basic_string<T> path, bool symbian_use,
        std::function<T(bool)> separator_func) {
        using generic_string = decltype(path);

        bool has_drive = (path.length() >= 2) && (path[1] == ':');
        bool has_net = is_separator(path[0]) && (path[0] == path[1]);

        if (has_drive) {
            if (path.size() > 2 && is_separator(path[2])) {
                return path.substr(0, 3);
            } else {
                return path.substr(0, 2);
            }
        } else if (has_net) {
            auto res = path.find_first_of(separator_func(symbian_use), 2);

            if (res == std::string::npos) {
                return generic_string{};
            }

            return path.substr(0, res);
        } else {
            if (is_separator(path[0])) {
                return path.substr(0, 1);
            }
        }

        return generic_string{};
    }

    template <typename T>
    std::basic_string<T> filename_impl(std::basic_string<T> path, bool symbian_use,
        std::function<T(bool)> separator_func) {
        using generic_string = decltype(path);
        generic_string fn;

        if (path.length() < 1) {
            return generic_string{};
        }

        if (is_separator(path[path.length() - 1])) {
            // It's directory
            return fn;
        }

        for (int64_t i = path.length(); i >= 0; --i) {
            if (is_separator(path[i])) {
                break;
            }

            fn = path[i] + fn;
        }

        return fn;
    }

    template <typename T>
    std::basic_string<T> file_directory_impl(std::basic_string<T> path, bool symbian_use) {
        using generic_string = decltype(path);
        auto fn = filename(path, symbian_use);

        if (fn == generic_string{}) {
            return path;
        }

        return path.substr(0, path.length() - fn.length() + 1);
    }

    std::string absolute_path(std::string str, std::string current_dir, bool symbian_use) {
        return absolute_path_impl<char>(str, current_dir, symbian_use, get_separator);
    }

    std::u16string absolute_path(std::u16string str, std::u16string current_dir, bool symbian_use) {
        return absolute_path_impl<char16_t>(str, current_dir, symbian_use, get_separator_16);
    }

    std::string relative_path(const std::string &str, bool symbian_use) {
        return relative_path_impl<char>(str, symbian_use);
    }

    std::u16string relative_path(const std::u16string &str, bool symbian_use) {
        return relative_path_impl<char16_t>(str, symbian_use);
    }

    std::string add_path(const std::string &path1, const std::string &path2, bool symbian_use) {
        return add_path_impl<char>(path1, path2, symbian_use, get_separator);
    }

    std::u16string add_path(const std::u16string &path1, const std::u16string &path2, bool symbian_use) {
        return add_path_impl<char16_t>(path1, path2, symbian_use, get_separator_16);
    }

    std::string filename(std::string path, bool symbian_use) {
        return filename_impl<char>(path, symbian_use, get_separator);
    }

    std::u16string filename(std::u16string path, bool symbian_use) {
        return filename_impl<char16_t>(path, symbian_use, get_separator);
    }

    std::string file_directory(std::string path, bool symbian_use) {
        return file_directory_impl<char>(path, symbian_use);
    }

    std::u16string file_directory(std::u16string path, bool symbian_use) {
        return file_directory_impl<char16_t>(path, symbian_use);
    }

    std::string root_name(std::string path, bool symbian_use) {
        return root_name_impl<char>(path, symbian_use, get_separator);
    }

    std::u16string root_name(std::u16string path, bool symbian_use) {
        return root_name_impl<char16_t>(path, symbian_use, get_separator_16);
    }

    std::string root_dir(std::string path, bool symbian_use) {
        return root_dir_impl<char>(path, symbian_use, get_separator);
    }

    std::u16string root_dir(std::u16string path, bool symbian_use) {
        return root_dir_impl<char16_t>(path, symbian_use, get_separator_16);
    }

    std::string root_path(std::string path, bool symbian_use) {
        return root_path_impl<char>(path, symbian_use, get_separator);
    }

    std::u16string root_path(std::u16string path, bool symbian_use) {
        return root_path_impl<char16_t>(path, symbian_use, get_separator_16);
    }

    std::string path_extension(const std::string &path) {
        return path_extension_impl<char>(path);
    }

    std::u16string path_extension(const std::u16string &path) {
        return path_extension_impl<char16_t>(path);
    }

    std::string replace_extension(const std::string &path, const std::string &new_ext) {
        return replace_extension_impl<char>(path, new_ext);
    }

    std::u16string replace_extension(const std::u16string &path, const std::u16string &new_ext) {
        return replace_extension_impl<char16_t>(path, new_ext);
    }

    void create_directory(std::string path) {
#if EKA2L1_PLATFORM(POSIX)
        mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#elif EKA2L1_PLATFORM(WIN32)
        CreateDirectoryA(path.c_str(), NULL);
#endif
    }

    bool exists(std::string path) {
#if EKA2L1_PLATFORM(POSIX)
        struct stat st;
        auto res = stat(path.c_str(), &st);

        return res != -1;
#elif EKA2L1_PLATFORM(WIN32)
        DWORD dw_attrib = GetFileAttributesA(path.c_str());
        return (dw_attrib != INVALID_FILE_ATTRIBUTES);
#endif
    }

    bool is_dir(std::string path) {
#if EKA2L1_PLATFORM(POSIX)
        struct stat st;
        auto res = stat(path.c_str(), &st);

        if (res < 0) {
            return false;
        }

        return S_ISDIR(st.st_mode);
#elif EKA2L1_PLATFORM(WIN32)
        DWORD dw_attrib = GetFileAttributesA(path.c_str());
        return (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
#endif
    }

    void create_directories(std::string path) {
        std::string crr_path = "";

        path_iterator ite;

        for (ite = path_iterator(path);
             ite; ite++) {
            crr_path = add_path(crr_path, add_path(*ite, "/"));

            if (!is_dir(crr_path)) {
                create_directory(crr_path);
            }
        }
    }

    bool set_current_directory(const std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        return SetCurrentDirectory(path.c_str());
#else
        return (chdir(path.c_str()) == 0);
#endif
    }

    bool get_current_directory(std::string &path) {
        path.resize(512);

#if EKA2L1_PLATFORM(WIN32)
        if (GetCurrentDirectoryA(static_cast<DWORD>(path.size()), &path[0]) == 0) {
            return false;
        }

        return true;
#else
        if (getcwd(&path[0], path.size()) == nullptr) {
            return false;
        }

        return true;
#endif
    }
}
