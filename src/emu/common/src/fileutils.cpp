/*
 * Copyright (c) 2018- EKA2L1 Team.
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

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/time.h>

#include <fstream>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#elif EKA2L1_PLATFORM(UNIX) || EKA2L1_PLATFORM(DARWIN)
#include <sys/stat.h>
#include <fnmatch.h>
#include <unistd.h>
#endif

#if EKA2L1_PLATFORM(POSIX)
#include <dirent.h>
#endif

#if EKA2L1_PLATFORM(UWP)
#include <common/cvt.h>
#endif

#include <stack>
#include <string.h>

namespace eka2l1::common {
    std::int64_t file_size(const std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        WIN32_FIND_DATA data;
        HANDLE h = FindFirstFileA(path.c_str(), &data);

        if (h == INVALID_HANDLE_VALUE) {
            return 0;
        }

        FindClose(h);

        return static_cast<std::int64_t>(data.nFileSizeLow | (__int64)data.nFileSizeHigh << 32);
#else
        struct stat st;
        auto res = stat(path.c_str(), &st);

        if (res == -1) {
            return res;
        }

        return static_cast<std::int64_t>(st.st_size);
#endif
    }

    static file_type get_file_type_from_attrib_platform_specific(const int att) {
#if EKA2L1_PLATFORM(WIN32)
        if (att & FILE_ATTRIBUTE_DIRECTORY) {
            return FILE_DIRECTORY;
        }

        if (att & FILE_ATTRIBUTE_DEVICE) {
            return FILE_DEVICE;
        }

        if ((att & FILE_ATTRIBUTE_NORMAL) || (att & FILE_ATTRIBUTE_ARCHIVE)) {
            return FILE_REGULAR;
        }

        return FILE_UNKN;
#elif EKA2L1_PLATFORM(POSIX)
        if (S_ISDIR(att)) {
            return FILE_DIRECTORY;
        }

        if (S_ISREG(att)) {
            return FILE_REGULAR;
        }

        return FILE_UNKN;
#endif
    }

    file_type get_file_type(const std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        DWORD h = GetFileAttributesA(path.c_str());

        if (h == INVALID_FILE_ATTRIBUTES) {
            return FILE_INVALID;
        }

        return get_file_type_from_attrib_platform_specific(h);
#else
        struct stat st;
        auto res = stat(path.c_str(), &st);

        if (res == -1) {
            return FILE_INVALID;
        }

        return get_file_type_from_attrib_platform_specific(st.st_mode);
#endif
    }

    bool is_file(const std::string &path, const file_type expected, file_type *result) {
        file_type res = get_file_type(path);

        if (result) {
            *result = res;
        }

        return res == expected;
    }

    dir_iterator::dir_iterator(const std::string &name)
        : handle(nullptr)
        , eof(false)
        , detail(false)
        , dir_name(name) {
#if EKA2L1_PLATFORM(POSIX)
        match_pattern = eka2l1::filename(dir_name);
        dir_name = eka2l1::file_directory(dir_name);

        if (match_pattern.empty()) {
            match_pattern = "*";
        }

        match_pattern += '\0';

        handle = reinterpret_cast<void *>(opendir(dir_name.c_str()));

        if (handle) {
            find_data = reinterpret_cast<void *>(readdir(reinterpret_cast<DIR *>(handle)));
        }

        struct dirent *d = reinterpret_cast<decltype(d)>(find_data);

        while (d && (fnmatch(match_pattern.c_str(), d->d_name, 0) != 0) && is_valid()) {
            cycles_to_next_entry();
            d = reinterpret_cast<decltype(d)>(find_data);
        };
#elif EKA2L1_PLATFORM(WIN32)
        find_data = new WIN32_FIND_DATA;

        std::string name_wildcard = name;

        if (eka2l1::filename(name).empty()) {
            name_wildcard += "\\*";
        }

        handle = reinterpret_cast<void *>(FindFirstFileA(name_wildcard.c_str(),
            reinterpret_cast<LPWIN32_FIND_DATA>(find_data)));

        if (handle == INVALID_HANDLE_VALUE) {
            handle = nullptr;

            WIN32_FIND_DATA *find_data_casted = reinterpret_cast<WIN32_FIND_DATA*>(find_data);
            delete find_data_casted;
        }
#endif
    }

    dir_iterator::~dir_iterator() {
        if (handle) {
#if EKA2L1_PLATFORM(WIN32)
            FindClose(reinterpret_cast<HANDLE>(handle));

            if (find_data) {
                delete find_data;
            }
#elif EKA2L1_PLATFORM(POSIX)
            closedir(reinterpret_cast<DIR *>(handle));
#endif
        }
    }

    void dir_iterator::cycles_to_next_entry() {
#if EKA2L1_PLATFORM(WIN32)
        DWORD result = FindNextFile(reinterpret_cast<HANDLE>(handle),
            reinterpret_cast<LPWIN32_FIND_DATA>(find_data));

        if (result == 0) {
            result = GetLastError();

            if (result == ERROR_NO_MORE_FILES) {
                eof = true;
            }
        }
#else
        find_data = reinterpret_cast<void *>(readdir(reinterpret_cast<DIR *>(handle)));

        if (!find_data) {
            eof = true;
        }
#endif
    }

    bool dir_iterator::is_valid() const {
        return (handle != nullptr) && (!eof);
    }

    int dir_iterator::next_entry(dir_entry &entry) {
        if (eof) {
            return -1;
        }

        if (!handle || !find_data) {
            return -2;
        }

#if EKA2L1_PLATFORM(WIN32)
        LPWIN32_FIND_DATA fdata_win32 = reinterpret_cast<decltype(fdata_win32)>(find_data);

        entry.name = fdata_win32->cFileName;

        if (detail) {
            entry.size = (fdata_win32->nFileSizeLow | (__int64)fdata_win32->nFileSizeHigh << 32);
            entry.type = get_file_type_from_attrib_platform_specific(fdata_win32->dwFileAttributes);
        }

        cycles_to_next_entry();
        fdata_win32 = reinterpret_cast<LPWIN32_FIND_DATA>(find_data);
#elif EKA2L1_PLATFORM(POSIX)
        struct dirent *d = reinterpret_cast<decltype(d)>(find_data);
        entry.name = d->d_name;

        if (detail) {
            entry.size = file_size(dir_name + "/" + entry.name);
            entry.type = get_file_type(dir_name + "/" + entry.name);
        }

        do {
            cycles_to_next_entry();
            d = reinterpret_cast<struct dirent *>(find_data);
        } while (d && (fnmatch(match_pattern.c_str(), d->d_name, 0) != 0));
#endif

        return 0;
    }

    int resize(const std::string &path, const std::uint64_t size) {
#if EKA2L1_PLATFORM(WIN32)
#if EKA2L1_PLATFORM(UWP)
        std::u16string path_ucs2 = common::utf8_to_ucs2(path);
        HANDLE h = CreateFile2(path_ucs2.c_str(), GENERIC_WRITE, 0, OPEN_EXISTING, nullptr);
#else
        HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
            nullptr);
#endif

        if ((!h) || (h == INVALID_HANDLE_VALUE)) {
            DWORD err = GetLastError();

            if (err == ERROR_FILE_NOT_FOUND) {
                return -1;
            }

            return -2;
        }

        LONG sizeHigh = static_cast<LONG>(size >> 32);
        DWORD set_pointer_result = SetFilePointer(h, static_cast<LONG>(size), &sizeHigh,
            FILE_BEGIN);

        if ((set_pointer_result != static_cast<DWORD>(size)) || ((size >> 32) != sizeHigh)) {
            CloseHandle(h);
            return -2;
        }

        BOOL resize_result = SetEndOfFile(h);

        if (!resize_result) {
            CloseHandle(h);
            return -2;
        }

        CloseHandle(h);
        return 0;
#else
        int result = truncate(path.c_str(), static_cast<off_t>(size));

        if (result == -1) {
            return -2;
        }

        return 0;
#endif
    }

    bool remove(const std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        if (path.back() == '\\' || path.back() == '/') {
            return RemoveDirectoryA(path.c_str());
        }

        return DeleteFileA(path.c_str());
#else
        return (::remove(path.c_str()) == 0);
#endif
    }

    bool move_file(const std::string &path, const std::string &new_path) {
#if EKA2L1_PLATFORM(WIN32)
        return MoveFileA(path.c_str(), new_path.c_str());
#else
        return (rename(path.c_str(), new_path.c_str()) == 0);
#endif
    }

    bool copy_file(const std::string &target_file, const std::string &dest, const bool overwrite_if_dest_exists) {
#if EKA2L1_PLATFORM(WIN32)
        return CopyFile(target_file.c_str(), dest.c_str(), !overwrite_if_dest_exists);
#else
        if (!eka2l1::exists(target_file)) {
            return false;
        }

        if (eka2l1::exists(dest) && !overwrite_if_dest_exists) {
            return false;
        }

        std::ifstream src(target_file, std::ios::binary);
        std::ofstream dst(dest, std::ios::binary | std::ios::trunc);

        if (src.fail() || dst.fail()) {
            return false;
        }

        dst << src.rdbuf();
        return true;
#endif
    }

    std::uint64_t get_last_modifiy_since_ad(const std::u16string &path) {
#if EKA2L1_PLATFORM(WIN32)
        WIN32_FILE_ATTRIBUTE_DATA attrib_data;
        if (GetFileAttributesExW(reinterpret_cast<const LPCWSTR>(path.c_str()), GetFileExInfoStandard, &attrib_data) == false) {
            return 0xFFFFFFFFFFFFFFFF;
        }

        FILETIME last_modify_time = attrib_data.ftLastWriteTime;

        // 100 nanoseconds = 0.1 microseconds
        return convert_microsecs_win32_1601_epoch_to_1ad(
            static_cast<std::uint64_t>(last_modify_time.dwLowDateTime) | (static_cast<std::uint64_t>(last_modify_time.dwHighDateTime) << 32));
#else
        const std::string name_utf8 = common::ucs2_to_utf8(path);
        struct stat st;
        auto res = stat(name_utf8.c_str(), &st);

        if (res == -1) {
            return 0xFFFFFFFFFFFFFFFF;
        }

        return convert_microsecs_epoch_to_1ad(static_cast<std::uint64_t>(st.st_mtime));
#endif
    }

    bool is_system_case_insensitive() {
#if EKA2L1_PLATFORM(WIN32)
        return true;
#else
        return false;
#endif
    }
    
    bool copy_folder(const std::string &target_folder, const std::string &dest_folder_to_reside, const std::uint32_t flags, std::atomic<int> *progress) {
        if (!exists(target_folder)) {
            return false;
        }

        bool no_copy = false;

        if (target_folder == dest_folder_to_reside) {
            no_copy = true;
        }

        std::uint64_t total_size = 0;
        std::uint64_t total_copied = 0;

        auto do_copy_stuffs = [&](const bool is_measuring) {
            std::stack<std::string> folder_stacks;
            common::dir_entry entry;

            folder_stacks.push(std::string(1, eka2l1::get_separator()));

            while (!folder_stacks.empty()) {
                if (!is_measuring)
                    eka2l1::create_directories(eka2l1::add_path(dest_folder_to_reside, folder_stacks.top()));

                const std::string top_path = folder_stacks.top();

                common::dir_iterator iterator(add_path(target_folder, top_path));
                iterator.detail = true;

                folder_stacks.pop();

                while (iterator.next_entry(entry) == 0) {
                    if ((entry.name == ".") || (entry.name == ".."))
                        continue;

                    std::string name_to_use = entry.name;
                    
                    if (!is_measuring) {
                        const auto lowercased = common::lowercase_string(entry.name);

                        if (flags & FOLDER_COPY_FLAG_LOWERCASE_NAME) {
                            if (no_copy) {
                                if (entry.name != lowercased) {
                                    if (!common::move_file(iterator.dir_name + entry.name, iterator.dir_name + lowercased)) {
                                        return false;
                                    }
                                    
                                    if (progress) {
                                        total_copied += entry.size;
                                        *progress = static_cast<int>(total_copied * 100 / total_size);
                                    }
                                }
                            }
                            
                            name_to_use = lowercased;
                        }
                    }

                    if (entry.type == common::file_type::FILE_DIRECTORY) {
                        folder_stacks.push(eka2l1::add_path(top_path, name_to_use + eka2l1::get_separator()));
                    } else {
                        if (is_measuring) {
                            total_size += entry.size;
                        } else {
                            if (!no_copy) {
                                if (!common::copy_file(iterator.dir_name + entry.name, eka2l1::add_path(dest_folder_to_reside, top_path) +
                                    eka2l1::get_separator() + name_to_use, true)) {
                                    return false;
                                }

                                if (progress) {
                                    total_copied += entry.size;
                                    *progress = static_cast<int>(total_copied * 100ULL / total_size);
                                }
                            }
                        };
                    }
                }
            }

            return true;
        };

        if (progress) {
            do_copy_stuffs(true);
        }

        return do_copy_stuffs(false);
    }

    bool delete_folder(const std::string& target_folder) {
        common::dir_iterator iterator(target_folder);
        iterator.detail = true;

        common::dir_entry entry;

        while (iterator.next_entry(entry) == 0) {
            std::string name = add_path(iterator.dir_name, entry.name);

            if (entry.type == common::file_type::FILE_DIRECTORY) {
                if ((entry.name != ".") && (entry.name != "..")) {
                    name += eka2l1::get_separator();

                    delete_folder(name);
                }
            }
            common::remove(name);
        }
        return common::remove(target_folder);
    }
}
