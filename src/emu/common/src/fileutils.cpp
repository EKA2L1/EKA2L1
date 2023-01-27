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
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/time.h>

#include <fstream>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#elif EKA2L1_PLATFORM(UNIX) || EKA2L1_PLATFORM(DARWIN)
#include <sys/stat.h>
#include <unistd.h>
#endif

#if EKA2L1_PLATFORM(POSIX)
#include <dirent.h>
#endif

#if EKA2L1_PLATFORM(ANDROID)
#include <common/android/contenturi.h>
#include <common/android/storage.h>
#endif

#include <stack>
#include <cstring>
#include <regex>

#include <common/pystr.h>
#include <common/wildcard.h>

namespace eka2l1::common {
    std::int64_t file_size(const std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        const std::wstring path_w = common::utf8_to_wstr(path);

        WIN32_FIND_DATAW data;
        HANDLE h = FindFirstFileW(path_w.c_str(), &data);

        if (h == INVALID_HANDLE_VALUE) {
            return 0;
        }

        FindClose(h);

        return static_cast<std::int64_t>(data.nFileSizeLow | (__int64)data.nFileSizeHigh << 32);
#else
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            std::optional<std::string> info = android::get_file_info_as_string(path);
            if (!info.has_value()) {
                return 0;
            }

            common::pystr str(info.value());
            std::vector<common::pystr> parts = str.split('|');
            if (parts.size() < 2) {
                return 0;
            }

            return parts[1].as_int(0, 10);
        } else
#endif
        {
            struct stat64 st;
            auto res = stat64(path.c_str(), &st);

            if (res == -1) {
                return res;
            }

            return static_cast<std::int64_t>(st.st_size);
        }
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
        const std::wstring path_w = common::utf8_to_wstr(path);
        DWORD h = GetFileAttributesW(path_w.c_str());

        if (h == INVALID_FILE_ATTRIBUTES) {
            return FILE_INVALID;
        }

        return get_file_type_from_attrib_platform_specific(h);
#else
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            std::optional<std::string> info = android::get_file_info_as_string(path);
            if (!info.has_value() || info->length() < 1) {
                return FILE_INVALID;
            }

            if (info->at(0) == 'D') {
                return FILE_DIRECTORY;
            }

            return FILE_REGULAR;
        } else
#endif
        {
            struct stat64 st;
            auto res = stat64(path.c_str(), &st);

            if (res == -1) {
                return FILE_INVALID;
            }

            return get_file_type_from_attrib_platform_specific(st.st_mode);
        }
#endif
    }

    bool is_file(const std::string &path, const file_type expected, file_type *result) {
        file_type res = get_file_type(path);

        if (result) {
            *result = res;
        }

        return res == expected;
    }

    struct standard_dir_iterator: public dir_iterator {
    protected:
        std::regex match_regex;

    public:
        void *handle;
        void *find_data;

        bool eof;

        void cycles_to_next_entry();

    public:
        explicit standard_dir_iterator(const std::string &name);
        ~standard_dir_iterator() override;

        bool is_valid() const override;

        /**
         * \brief Get the next entry of the folder
         * \returns 0 if success
         *          -1 if EOF
         */
        int next_entry(dir_entry &entry) override;
    };

    standard_dir_iterator::standard_dir_iterator(const std::string &name)
        : dir_iterator(name)
        , handle(nullptr)
        , eof(false) {
        dir_name = transform_separators<char>(dir_name, false, get_separator);
        detail = false;

#if EKA2L1_PLATFORM(POSIX)
        std::string match_pattern = eka2l1::filename(dir_name);
        dir_name = eka2l1::file_directory(dir_name);

        if (match_pattern.empty()) {
            match_pattern = "*";
        }

        match_regex = std::regex(common::wildcard_to_regex_string(match_pattern), std::regex_constants::icase);
        handle = reinterpret_cast<void *>(opendir(dir_name.c_str()));

        if (handle) {
            find_data = reinterpret_cast<void *>(readdir(reinterpret_cast<DIR *>(handle)));
        } else {
            find_data = nullptr;
        }

        struct dirent *d = reinterpret_cast<decltype(d)>(find_data);

        while (d && !std::regex_match(d->d_name, match_regex) && is_valid()) {
            cycles_to_next_entry();
            d = reinterpret_cast<decltype(d)>(find_data);
        };
#elif EKA2L1_PLATFORM(WIN32)
        find_data = new WIN32_FIND_DATAW;

        std::string name_wildcard = name;

        if (eka2l1::filename(name).empty()) {
            name_wildcard += "\\*";
        }

        const std::wstring name_wildcard_w = common::utf8_to_wstr(name_wildcard);

        handle = reinterpret_cast<void *>(FindFirstFileExW(name_wildcard_w.c_str(), FindExInfoBasic,
            reinterpret_cast<LPWIN32_FIND_DATAW>(find_data), FindExSearchNameMatch, NULL,
            FIND_FIRST_EX_LARGE_FETCH));

        if (handle == INVALID_HANDLE_VALUE) {
            handle = nullptr;

            WIN32_FIND_DATAW *find_data_casted = reinterpret_cast<WIN32_FIND_DATAW *>(find_data);
            delete find_data_casted;
        }
#endif
    }

    standard_dir_iterator::~standard_dir_iterator() {
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

    void standard_dir_iterator::cycles_to_next_entry() {
#if EKA2L1_PLATFORM(WIN32)
        DWORD result = FindNextFileW(reinterpret_cast<HANDLE>(handle),
            reinterpret_cast<LPWIN32_FIND_DATAW>(find_data));

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

    bool standard_dir_iterator::is_valid() const {
        return (handle != nullptr) && (!eof);
    }

    int standard_dir_iterator::next_entry(dir_entry &entry) {
        if (eof) {
            return -1;
        }

        if (!handle || !find_data) {
            return -2;
        }

#if EKA2L1_PLATFORM(WIN32)
        LPWIN32_FIND_DATAW fdata_win32 = reinterpret_cast<decltype(fdata_win32)>(find_data);

        entry.name = common::wstr_to_utf8(fdata_win32->cFileName);

        if (detail) {
            entry.size = (fdata_win32->nFileSizeLow | (__int64)fdata_win32->nFileSizeHigh << 32);
            entry.type = get_file_type_from_attrib_platform_specific(fdata_win32->dwFileAttributes);
        }

        cycles_to_next_entry();
        fdata_win32 = reinterpret_cast<LPWIN32_FIND_DATAW>(find_data);
#elif EKA2L1_PLATFORM(POSIX)
        struct dirent *d = reinterpret_cast<decltype(d)>(find_data);
        entry.name = d->d_name;

        if (detail) {
            const std::string path_full = dir_name + "/" + entry.name;

            struct stat64 st;
            auto res = stat64(path_full.c_str(), &st);

            if (res != -1) {
                entry.size = st.st_size;
                entry.type = get_file_type_from_attrib_platform_specific(st.st_mode);
            }
        }

        do {
            cycles_to_next_entry();
            d = reinterpret_cast<struct dirent *>(find_data);
        } while (d && !std::regex_match(d->d_name, match_regex));
#endif

        return 0;
    }

#if EKA2L1_PLATFORM(ANDROID)
    static bool parse_uri_entry_info(const std::string &info, dir_entry &entry) {
        common::pystr infopy(info);
        std::vector<common::pystr> comps = infopy.split('|');
        if (comps.size() != 4) {
            LOG_ERROR(COMMON, "Can't parse URI content entry info: {}", info);
            return false;
        }
        entry.name = comps[2].std_str();
        entry.size = comps[1].as_int(0, 10);
        entry.type = FILE_UNKN;

        if (!comps[0].empty()) {
            entry.type = (comps[0][0] == 'D') ? FILE_DIRECTORY : FILE_REGULAR;
        }

        return true;
    }

    // There's sadly no direct C/C++ interface. If we follow the when needed retrieve next model,
    // IPC + Java layer will create a horrible performance cost. There are better way to do this
    // though.
    struct content_uri_dir_iterator: public dir_iterator {
    private:
        std::vector<std::string> file_infos_;
        std::size_t current_index_;

    public:
        explicit content_uri_dir_iterator(const std::string &name)
            : dir_iterator(name)
            , current_index_(0) {
            file_infos_ = android::list_content_uri(name);
        }

        ~content_uri_dir_iterator() override = default;

        bool is_valid() const {
            return (current_index_ < file_infos_.size());
        }

        int next_entry(dir_entry &entry) {
            if (current_index_ >= file_infos_.size()) {
                return -1;
            }

            if (!parse_uri_entry_info(file_infos_[current_index_++], entry)) {
                return -2;
            }

            return 0;
        }
    };
#endif

    std::unique_ptr<dir_iterator> make_directory_iterator(const std::string &path) {
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            return std::make_unique<content_uri_dir_iterator>(path);
        }
#endif

        return std::make_unique<standard_dir_iterator>(path);
    }
    
    std::string find_case_sensitive_file_name(const std::string &folder_path, const std::string &insensitive_name, const file_type type) {
        auto ite = make_directory_iterator(folder_path);
        const std::u16string insensitive_name_u16 = common::utf8_to_ucs2(insensitive_name);

        common::dir_entry entry;

        while (ite->next_entry(entry) == 0) {
            if (type == entry.type) {
                if (common::compare_ignore_case(common::utf8_to_ucs2(entry.name), insensitive_name_u16) == 0) {
                    return entry.name;
                }
            }
        }

        return "";
    }

    int resize(const std::string &path, const std::uint64_t size) {
#if EKA2L1_PLATFORM(WIN32)
#if EKA2L1_PLATFORM(UWP)
        std::u16string path_ucs2 = common::utf8_to_ucs2(path);
        HANDLE h = CreateFile2(path_ucs2.c_str(), GENERIC_WRITE, 0, OPEN_EXISTING, nullptr);
#else
        const std::wstring path_w = common::utf8_to_wstr(path);
        HANDLE h = CreateFileW(path_w.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
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
        const std::wstring path_w = common::utf8_to_wstr(path);

        if (path_w.back() == L'\\' || path_w.back() == L'/') {
            return RemoveDirectoryW(path_w.c_str());
        }

        return DeleteFileW(path_w.c_str());
#else
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            return (android::remove_file(path) == android::storage_error::SUCCESS);
        } else
#endif
        return (::remove(path.c_str()) == 0);
#endif
    }

    bool move_file(const std::string &path, const std::string &new_path) {
#if EKA2L1_PLATFORM(WIN32)
        const std::wstring path_s_w = common::utf8_to_wstr(path);
        const std::wstring path_d_w = common::utf8_to_wstr(new_path);

        return MoveFileW(path_s_w.c_str(), path_d_w.c_str());
#else
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path) && is_content_uri(new_path)) {
            android::content_uri parent(path);

            if (parent.navigate_up()) {
                return (android::move_file(path, parent.to_string(), new_path)
                        == android::storage_error::SUCCESS);
            } else {
                LOG_ERROR(COMMON, "Failed to navigate up parent content URI to move file!");
                return false;
            }
        } else
#endif
        {
            if (rename(path.c_str(), new_path.c_str()) == 0) {
                return true;
            } else if (copy_file(path, new_path, true)) {
                return remove(path);
            } else {
                return false;
            }
        }
#endif
    }

    bool copy_file(const std::string &target_file, const std::string &dest, const bool overwrite_if_dest_exists) {
#if EKA2L1_PLATFORM(WIN32)
        const std::wstring path_s_w = common::utf8_to_wstr(target_file);
        const std::wstring path_d_w = common::utf8_to_wstr(dest);

        return CopyFileW(path_s_w.c_str(), path_d_w.c_str(), !overwrite_if_dest_exists);
#else
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(target_file) && is_content_uri(dest)) {
            return (android::copy_file(target_file, dest) == android::storage_error::SUCCESS);
        } else
#endif
        {
#define BSIZE 16384
            char buffer[BSIZE];

            // Open input file
            FILE *input = open_c_file(target_file, "rb");
            if (!input) {
                return false;
            }

            // open output file
            FILE *output = open_c_file(dest, "wb");
            if (!output) {
                fclose(input);
                return false;
            }

            // copy loop
            while (!feof(input)) {
                // read input
                int rnum = fread(buffer, sizeof(char), BSIZE, input);
                if (rnum != BSIZE) {
                    if (ferror(input) != 0) {
                        fclose(input);
                        fclose(output);
                        return false;
                    }
                }

                // write output
                int wnum = fwrite(buffer, sizeof(char), rnum, output);
                if (wnum != rnum) {
                    fclose(input);
                    fclose(output);
                    return false;
                }
            }
            // close flushes
            fclose(input);
            fclose(output);
            return true;
        }
#endif
    }

    std::uint64_t get_last_modifiy_since_ad(const std::u16string &path) {
#if EKA2L1_PLATFORM(WIN32)
        const std::wstring path_w = common::ucs2_to_wstr(path);

        WIN32_FILE_ATTRIBUTE_DATA attrib_data;
        if (GetFileAttributesExW(path_w.c_str(), GetFileExInfoStandard, &attrib_data) == false) {
            return 0xFFFFFFFFFFFFFFFF;
        }

        FILETIME last_modify_time = attrib_data.ftLastWriteTime;

        // 100 nanoseconds = 0.1 microseconds
        return convert_microsecs_win32_1601_epoch_to_0ad(
            static_cast<std::uint64_t>(last_modify_time.dwLowDateTime) | (static_cast<std::uint64_t>(last_modify_time.dwHighDateTime) << 32));
#else
        const std::string name_utf8 = common::ucs2_to_utf8(path);
#if EKA2L1_PLATFORM(ANDROID)
        if (eka2l1::is_content_uri(name_utf8)) {
            std::optional<std::string> info = android::get_file_info_as_string(name_utf8);
            if (!info.has_value()) {
                return 0;
            }

            common::pystr str(info.value());
            std::vector<common::pystr> parts = str.split('|');
            if (parts.size() < 4) {
                return 0;
            }

            // TODO: May not be microseconds, but no reason not to use EPOCH ;)
            return convert_microsecs_epoch_to_0ad(parts[3].as_int(0, 10));
        } else
#endif
        {
            struct stat64 st;
            auto res = stat64(name_utf8.c_str(), &st);

            if (res == -1) {
                return 0xFFFFFFFFFFFFFFFF;
            }

            return convert_microsecs_epoch_to_0ad(static_cast<std::uint64_t>(st.st_mtime));
        }
#endif
    }

    void create_directory(std::string path) {
#if EKA2L1_PLATFORM(POSIX)
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            android::content_uri uri(path);
            const std::string folder_name = uri.get_last_part();

            if (uri.navigate_up()) {
                android::create_directory(uri.to_string(), folder_name);
            } else {
                LOG_ERROR(COMMON, "Failed to navigate up URI to create directory!");
            }
        } else
#endif
        mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#elif EKA2L1_PLATFORM(WIN32)
        const std::wstring wpath = common::utf8_to_wstr(path);
        CreateDirectoryW(wpath.c_str(), NULL);
#endif
    }

    bool exists(std::string path) {
#if EKA2L1_PLATFORM(POSIX)
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path))
            return android::file_exists(path);
#endif
        struct stat st;
        auto res = stat(path.c_str(), &st);

        return res != -1;
#elif EKA2L1_PLATFORM(WIN32)
        const std::wstring wpath = common::utf8_to_wstr(path);
        DWORD dw_attrib = GetFileAttributesW(wpath.c_str());
        return (dw_attrib != INVALID_FILE_ATTRIBUTES);
#endif
    }

    bool is_dir(std::string path) {
#if EKA2L1_PLATFORM(POSIX)
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            std::optional<std::string> info = android::get_file_info_as_string(path);
            if (!info.has_value() || info->length() < 1) {
                return false;
            }

            if (info->at(0) == 'D') {
                return true;
            }

            return false;
        }
#endif
        struct stat st;
        auto res = stat(path.c_str(), &st);

        if (res < 0) {
            return false;
        }

        return S_ISDIR(st.st_mode);
#elif EKA2L1_PLATFORM(WIN32)
        const std::wstring wpath = common::utf8_to_wstr(path);
        DWORD dw_attrib = GetFileAttributesW(wpath.c_str());
        return (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
#endif
    }

    void create_directories(std::string path) {
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(path)) {
            android::content_uri uri = android::content_uri(path);
            android::content_uri root = uri.with_root_file_path("");
            std::string diff;
            if (!root.compute_path_to(uri, diff)) {
                return;
            }

            std::vector<common::pystr> parts = common::pystr(diff).split('/');
            android::content_uri cur_uri = android::content_uri(root);
            for (auto &part : parts) {
                cur_uri = cur_uri.with_component(part.std_str());
                std::string uri_path = cur_uri.to_string();
                if (!android::file_exists(uri_path)) {
                    create_directory(uri_path);
                }
            }
        } else
#endif
        {
            std::string crr_path;

            path_iterator ite;

            for (ite = path_iterator(path);
                 ite; ite++) {
                crr_path = add_path(crr_path, add_path(*ite, "/"));

                if (get_file_type(crr_path) != file_type::FILE_DIRECTORY) {
                    create_directory(crr_path);
                }
            }
        }
    }

    bool set_current_directory(const std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        const std::wstring wpath = common::utf8_to_wstr(path);
        return SetCurrentDirectoryW(wpath.c_str());
#else
        return (chdir(path.c_str()) == 0);
#endif
    }

    bool get_current_directory(std::string &path) {
#if EKA2L1_PLATFORM(WIN32)
        std::wstring buffer(512, L'0');
        const DWORD written = GetCurrentDirectoryW(static_cast<DWORD>(buffer.length()), reinterpret_cast<wchar_t*>(buffer.data()));

        if (written == 0) {
            return false;
        }

        buffer.resize(written);
        path = common::wstr_to_utf8(buffer);
#else
        char buffer[512];

        if (getcwd(buffer, sizeof(buffer)) == nullptr) {
            return false;
        }

        path = std::string(buffer);
#endif

        return true;
    }

    bool is_system_case_insensitive() {
#if EKA2L1_PLATFORM(WIN32)
        return true;
#else
        return false;
#endif
    }

    bool copy_folder(const std::string &target_folder, const std::string &dest_folder_to_reside, const std::uint32_t flags, progress_changed_callback progress_cb,
        cancel_requested_callback cancel_cb) {
        if (!exists(target_folder)) {
            return false;
        }

        bool no_copy = false;

        if (target_folder == dest_folder_to_reside) {
            no_copy = true;
        }

        const bool overwrite_on_file_exist = ((flags & FOLDER_COPY_FLAG_FILE_NO_OVERWRITE_IF_EXIST) == 0);

        std::uint64_t total_size = 0;
        std::uint64_t total_copied = 0;

        auto do_copy_stuffs = [&](const bool is_measuring) {
            std::stack<std::string> folder_stacks;
            common::dir_entry entry;

            folder_stacks.push(std::string(1, eka2l1::get_separator()));

            while (!folder_stacks.empty()) {
                if (cancel_cb && cancel_cb()) {
                    break;
                }
                if (!is_measuring)
                    create_directories(eka2l1::add_path(dest_folder_to_reside, folder_stacks.top()));

                const std::string top_path = folder_stacks.top();

                auto iterator = make_directory_iterator(add_path(target_folder, top_path));
                if (!iterator) {
                    return false;
                }
                iterator->detail = true;

                folder_stacks.pop();

                while (iterator->next_entry(entry) == 0) {
                    if (cancel_cb && cancel_cb()) {
                        break;
                    }

                    if ((entry.name == ".") || (entry.name == ".."))
                        continue;

                    std::string name_to_use = entry.name;

                    if (!is_measuring) {
                        const auto lowercased = common::lowercase_string(entry.name);

                        if (flags & FOLDER_COPY_FLAG_LOWERCASE_NAME) {
                            if (no_copy) {
                                if (entry.name != lowercased) {
                                    if (!common::move_file(iterator->dir_name + entry.name, iterator->dir_name + lowercased)) {
                                        return false;
                                    }

                                    if (progress_cb) {
                                        total_copied += entry.size;
                                        progress_cb(total_copied, total_size);
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
                                if (cancel_cb && cancel_cb()) {
                                    continue;
                                }

                                if (!common::copy_file(iterator->dir_name + entry.name, eka2l1::add_path(dest_folder_to_reside, top_path) + eka2l1::get_separator() + name_to_use, overwrite_on_file_exist)) {
                                    return false;
                                }

                                if (progress_cb) {
                                    total_copied += entry.size;
                                    progress_cb(total_copied, total_size);
                                }
                            }
                        };
                    }
                }
            }

            return true;
        };

        if (progress_cb) {
            do_copy_stuffs(true);
        }

        return do_copy_stuffs(false);
    }

    bool delete_folder(const std::string &target_folder) {
        if (!exists(target_folder)) {
            return true;
        }

        auto iterator = make_directory_iterator(target_folder);
        if (!iterator) {
            return false;
        }

        iterator->detail = true;

        common::dir_entry entry;

        while (iterator->next_entry(entry) == 0) {
            std::string name = add_path(iterator->dir_name, entry.name);

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

    FILE *open_c_file(const std::string &target_file, const char *mode) {
#if EKA2L1_PLATFORM(ANDROID)
        if (is_content_uri(target_file)) {
            android::content_uri uri(target_file);
            if (!strcmp(mode, "r") || !strcmp(mode, "rb") || !strcmp(mode, "rt")) {
                int descriptor = android::open_content_uri_fd(target_file, android::open_content_uri_mode::READ);
                if (descriptor < 0) {
                    return nullptr;
                }

                return fdopen(descriptor, mode);
            } else if (!strcmp(mode, "w") || !strcmp(mode, "wb") || !strcmp(mode, "wt") || !strcmp(mode, "at") || !strcmp(mode, "a")) {
                if (!android::file_exists(target_file)) {
                    const std::string filename = uri.get_last_part();
                    if (uri.can_navigate_up()) {
                        android::content_uri parent(target_file);
                        parent.navigate_up();

                        if (android::create_file(parent.to_string(), filename) != android::storage_error::SUCCESS) {
                            return nullptr;
                        }
                    } else {
                        return nullptr;
                    }
                }

                // After creating the file, we can open it?
                int descriptor = android::open_content_uri_fd(target_file, android::open_content_uri_mode::READ);
                if (descriptor < 0) {
                    return nullptr;
                }

                FILE *result = fdopen(descriptor, mode);

                if (!result) {
                    return nullptr;
                }

                if (!strcmp(mode, "a") || !strcmp(mode, "at")) {
                    fseek(result, 0, SEEK_END);
                }

                return result;
            } else {
                LOG_ERROR(COMMON, "Unsupported file mode {} to open content file {}", mode,
                          target_file);
                return nullptr;
            }
        }
#endif

#if EKA2L1_PLATFORM(WIN32)
        std::wstring target_file_w = common::utf8_to_wstr(target_file);
        std::wstring target_mode_w = common::utf8_to_wstr(mode);

        return _wfopen(target_file_w.c_str(), target_mode_w.c_str());
#else
        return fopen(target_file.c_str(), mode);
#endif
    }
}
