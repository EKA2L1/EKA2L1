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

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/wildcard.h>

#include <loader/rom.h>
#include <mem/mem.h>
#include <mem/ptr.h>
#include <vfs/vfs.h>

#include <array>
#include <cwctype>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <thread>
#include <stack>

#include <string.h>

namespace eka2l1 {
    file::file(const std::uint32_t attrib)
        : io_component(io_component_type::file, attrib) {
    }

    drive::drive(const std::uint32_t attrib)
        : io_component(io_component_type::drive, attrib) {
    }

    directory::directory(const std::uint32_t attrib)
        : io_component(io_component_type::dir, attrib) {
    }

    bool file::flush() {
        return true;
    }

    std::size_t file::read_file(const std::uint64_t offset, void *buf, std::uint32_t size,
        std::uint32_t count) {
        const std::uint64_t last_offset = tell();

        seek(offset, file_seek_mode::beg);
        const std::size_t byte_readed = read_file(buf, size, count);

        seek(last_offset, file_seek_mode::beg);

        return byte_readed;
    }

    // Class for some one want to access rom
    struct rom_file : public file {
        loader::rom_entry file;
        loader::rom *parent;

        uint64_t crr_pos;
        std::mutex mut;

        memory_system *mem;

        std::uint8_t *file_ptr;

        rom_file(memory_system *mem, loader::rom *supreme_mother, loader::rom_entry entry)
            : parent(supreme_mother)
            , file(entry)
            , mem(mem) {
            file_ptr = ptr<std::uint8_t>(file.address_lin).get(mem);
            crr_pos = 0;
        }

        uint64_t size() const override {
            return file.size;
        }

        bool valid() override {
            return crr_pos < file.size;
        }

        size_t read_file(void *data, uint32_t size, uint32_t count) override {
            auto will_read = std::min((uint64_t)count * size, file.size - crr_pos);
            memcpy(data, &file_ptr[crr_pos], will_read);

            crr_pos += will_read;

            return static_cast<int>(will_read);
        }

        int file_mode() const override {
            return READ_MODE;
        }

        size_t write_file(const void *data, uint32_t size, uint32_t count) override {
            LOG_ERROR("Can't write into ROM!");
            return -1;
        }

        std::uint64_t seek(std::int64_t seek_off, file_seek_mode where) override {
            if (where == file_seek_mode::beg || where == file_seek_mode::address) {
                if (seek_off < 0) {
                    LOG_ERROR("Attempting to seek set with negative offset ({})", seek_off);
                    return 0xFFFFFFFFFFFFFFFF;
                }

                crr_pos = seek_off;
            } else if (where == file_seek_mode::crr) {
                if (crr_pos + seek_off < 0) {
                    LOG_ERROR("Attempting to seek current with offset that makes file pointer negative ({})", seek_off);
                    return 0xFFFFFFFFFFFFFFFF;
                }

                crr_pos += seek_off;
            } else {
                if (crr_pos + size() + seek_off < 0) {
                    LOG_ERROR("Attempting to seek end with offset that makes file pointer negative ({})", seek_off);
                    return 0xFFFFFFFFFFFFFFFF;
                }

                crr_pos = size() + seek_off;
            }

            if (where == file_seek_mode::address) {
                return file.address_lin + crr_pos;
            }

            return crr_pos;
        }

        std::uint64_t last_modify_since_1ad() override {
            return parent->header.time;
        }

        std::string get_error_descriptor() override {
            return "no";
        }

        bool is_in_rom() const override {
            return true;
        }

        address rom_address() const override {
            return file.address_lin;
        }

        uint64_t tell() override {
            return crr_pos;
        }

        std::u16string file_name() const override {
            return file.name;
        }

        bool close() override {
            return true;
        }

        bool resize(const std::size_t new_size) override {
            return false;
        }
    };

    struct physical_file : public file {
        FILE *file;

        std::u16string input_name;
        std::u16string physical_path;

        int fmode;

        bool closed;

        const char *translate_mode(int mode) {
            if (mode & READ_MODE) {
                if (mode & BIN_MODE) {
                    if (mode & WRITE_MODE) {
                        return "rb+";
                    }

                    return "rb";
                } else if (mode & WRITE_MODE) {
                    return "r+";
                }

                return "r";
            } else if (mode & WRITE_MODE) {
                if (mode & BIN_MODE) {
                    return "wb+";
                }

                return "w";
            } else if (mode & APPEND_MODE) {
                if (mode & BIN_MODE) {
                    return "ab+";
                }

                return "a";
            }

            return "";
        }

        const char16_t *translate_mode16(int mode) {
            if (mode & READ_MODE) {
                if (mode & BIN_MODE) {
                    if (mode & WRITE_MODE) {
                        return u"rb+";
                    }

                    return u"rb";
                } else if (mode & WRITE_MODE) {
                    return u"r+";
                }

                return u"r";
            } else if (mode & WRITE_MODE) {
                if (mode & BIN_MODE) {
                    return u"wb+";
                }

                return u"w";
            } else if (mode & APPEND_MODE) {
                if (mode & BIN_MODE) {
                    return u"ab+";
                }

                return u"a";
            }

            return u"";
        }

#define WARN_CLOSE \
    if (closed)    \
        LOG_WARN("File {} closed but operation still continues", common::ucs2_to_utf8(input_name));

        physical_file(const utf16_str &vfs_path, const utf16_str &real_path, const int mode)
            : file(nullptr) {
            init(vfs_path, real_path, mode);
        }

        ~physical_file() override {
            shutdown();
        }

        bool valid() override {
            return file && !feof(file);
        }

        int file_mode() const override {
            return fmode;
        }

        void init(const utf16_str &vfs_path, const utf16_str &real_path, const int mode) {
            // Disable directory check here
            closed = false;

            const char *cmode = translate_mode(mode);
            file = fopen(common::ucs2_to_utf8(real_path).c_str(), cmode);

            physical_path = real_path;

            // LOG_TRACE("Open with mode: {}", cmode);

            if (!file) {
                LOG_ERROR("Can't open file: {}", common::ucs2_to_utf8(real_path));
                return;
            }

            input_name = vfs_path;
            fmode = mode;
        }

        void shutdown() {
            if (file && !closed) {
                fclose(file);
            }
        }

        size_t write_file(const void *data, uint32_t size, uint32_t count) override {
            WARN_CLOSE

            return fwrite(data, size, count, file) * size;
        }

        size_t read_file(void *data, uint32_t size, uint32_t count) override {
            WARN_CLOSE

            return fread(data, size, count, file) * size;
        }

        std::uint64_t size() const override {
            WARN_CLOSE

            auto crr_pos = ftell(file);
            fseek(file, 0, SEEK_END);

            const std::uint64_t file_size = ftell(file);
            fseek(file, crr_pos, SEEK_SET);

            return file_size;
        }

        bool close() override {
            WARN_CLOSE

            fclose(file);
            closed = true;

            return true;
        }

        uint64_t tell() override {
            WARN_CLOSE

            return ftell(file);
        }

        std::uint64_t seek(std::int64_t seek_off, file_seek_mode where) override {
            WARN_CLOSE

            if (where == file_seek_mode::address) {
                return 0xFFFFFFFFFFFFFFFF;
            }

            if (where == file_seek_mode::beg) {
                if (seek_off < 0) {
                    LOG_ERROR("Attempting to seek set with negative offset ({})", seek_off);
                    return 0xFFFFFFFFFFFFFFFF;
                }

                fseek(file, static_cast<long>(seek_off), SEEK_SET);
            } else if (where == file_seek_mode::crr) {
                fseek(file, static_cast<long>(seek_off), SEEK_CUR);
            } else {
                fseek(file, static_cast<long>(seek_off), SEEK_END);
            }

            return ftell(file);
        }

        std::u16string file_name() const override {
            WARN_CLOSE

            return input_name;
        }

        bool flush() override {
            WARN_CLOSE

            return (fflush(file) == 0);
        }

        bool resize(const std::size_t new_size) override {
            if (!(fmode & WRITE_MODE)) {
                return false;
            }

            // Temporary close the file to let resize function works.
            const std::uint64_t saved_pos = tell();
            fclose(file);

            int err_code = common::resize(common::ucs2_to_utf8(physical_path), new_size);
            const std::string path_utf8 = common::ucs2_to_utf8(physical_path);

            // Reopen the file again...
            file = fopen(path_utf8.c_str(), translate_mode(fmode));
            fseek(file, static_cast<long>(saved_pos), SEEK_SET);

            return (err_code != 0) ? false : true;
        }

        std::uint64_t last_modify_since_1ad() override {
            return common::get_last_modifiy_since_ad(physical_path);
        }

        std::string get_error_descriptor() override {
            return "no";
        }

        bool is_in_rom() const override {
            return false;
        }

        address rom_address() const override {
            return 0;
        }
    };

    /* DIRECTORY VFS */
    class physical_directory : public directory {
        std::regex filter;
        std::string vir_path;

        common::dir_iterator iterator;
        common::dir_entry entry;

        std::optional<entry_info> peek_info;
        bool peeking;

        std::uint32_t attrib;

        abstract_file_system *inst;

    public:
        physical_directory(abstract_file_system *inst, const std::string &phys_path,
            const std::string &vir_path, const std::string &filter, const std::uint32_t attrib)
            : filter(common::wildcard_to_regex_string(common::lowercase_string(filter)))
            , iterator(phys_path)
            , vir_path(vir_path)
            , attrib(attrib)
            , inst(inst)
            , peeking(false) {
            iterator.detail = true;
        }

        std::optional<entry_info> get_next_entry() override {
            if (peeking) {
                peeking = false;
                return peek_info;
            }

            while (true) {
                if (!iterator.is_valid()) {
                    return std::optional<entry_info>{};
                }

                std::string name = "";
                int error_code = iterator.next_entry(entry);

                if (error_code != 0) {
                    return std::optional<entry_info>{};
                }

                name = entry.name;

                if (attrib != io_attrib_none) {
                    if (!(attrib & io_attrib_include_dir) && entry.type == common::FILE_DIRECTORY) {
                        continue;
                    }

                    if (!(attrib & io_attrib_include_file) && entry.type == common::FILE_REGULAR) {
                        continue;
                    }
                }

                // Quick hack: Regex dumb with null-terminated string
                if (name.back() == '\0') {
                    name.erase(name.length() - 1);
                }

                // If it doesn't meet the filter, continue until find one or there is no one
                if (!std::regex_match(common::lowercase_string(name), filter)) {
                    continue;
                }

                entry_info info = *(inst->get_entry_info(common::utf8_to_ucs2(
                    eka2l1::add_path(vir_path, name))));

                // Symbian usually sensitive about null terminator.
                // It's best not include them.
                if (info.name.back() == '\0') {
                    info.name.erase(info.name.length() - 1);
                }

                if (info.full_path.back() == '\0') {
                    info.full_path.erase(info.full_path.length() - 1);
                }

                return info;
            }

            return std::optional<entry_info>{};
        }

        std::optional<entry_info> peek_next_entry() override {
            if (!peeking) {
                peek_info = get_next_entry();
                peeking = true;
            }

            return peek_info;
        }
    };

    class physical_file_system : public abstract_file_system {
        std::mutex fs_mutex;
        std::unique_ptr<common::directory_watcher> watcher_;

    protected:
        std::string firmcode;
        epocver ver;

        // Use a flat array for drive mapping
        std::array<std::pair<drive, bool>, drive_z + 1> mappings;
        std::map<drive_number, std::vector<std::int64_t>> watches;

        constexpr char drive_number_to_ascii(const drive_number drv) {
            return static_cast<char>(drv) + 0x61;
        }

        constexpr drive_number ascii_to_drive_number(const char c) {
            return static_cast<drive_number>(c - 0x61);
        }

        bool do_mount(const drive_number drv, const drive_media media, const std::uint32_t attrib,
            const std::u16string &physical_path) {
            const std::lock_guard<std::mutex> guard(fs_mutex);

            if (mappings[static_cast<int>(drv)].second) {
                return false;
            }

            drive &map_drive = mappings[static_cast<int>(drv)].first;

            map_drive.attribute = attrib;
            map_drive.type = io_component_type::drive;
            map_drive.drive_name += drive_number_to_ascii(drv) + ':';
            map_drive.media_type = media;
            map_drive.real_path = common::ucs2_to_utf8(physical_path);

            if (!eka2l1::is_separator(map_drive.real_path.back())) {
                map_drive.real_path += eka2l1::get_separator(false);
            }

            // Mark as mapped
            mappings[static_cast<int>(drv)].second = true;

            return true;
        }

        std::optional<std::u16string> get_real_physical_path(const std::u16string &vert_path) {
            std::string path_ucs8 = common::ucs2_to_utf8(vert_path);
            const std::string root = eka2l1::root_name(path_ucs8);
            std::u16string vert_path_copy = vert_path;

            if (root == "" || !mappings[ascii_to_drive_number(static_cast<char>(std::towlower(root[0])))].second) {
                return std::nullopt;
            }

            drive &drv = mappings[ascii_to_drive_number(static_cast<char>(std::towlower(root[0])))].first;
            std::u16string map_path = common::utf8_to_ucs2(drv.real_path);

            if (!eka2l1::is_separator(static_cast<char>(map_path.back()))) {
                map_path += static_cast<char16_t>(eka2l1::get_separator());
            }

            if (drv.media_type == drive_media::rom) {
                if (firmcode.empty()) {
                    LOG_ERROR("IO error: No device has been set for the emulator.");
                    return u"";
                }

                map_path += common::utf8_to_ucs2(common::lowercase_string(firmcode));
            }

            if (static_cast<int>(ver) > static_cast<int>(epocver::epoc6)) {
                if (common::compare_ignore_case(u"\\system\\libs", vert_path_copy.substr(2, 12)) == 0) {
                    vert_path_copy.replace(2, 12, u"\\sys\\bin");
                } else if (common::compare_ignore_case(u"\\system\\programs", vert_path_copy.substr(2, 16)) == 0) {
                    vert_path_copy.replace(2, 16, u"\\sys\\bin");
                }
            }

            std::u16string vert_path_no_root = vert_path_copy.substr(root.size());

            if (!common::is_system_case_insensitive()) {
                vert_path_no_root = common::lowercase_ucs2_string(vert_path_no_root);
            }

            return eka2l1::add_path(map_path, vert_path_no_root);
        }

    public:
        explicit physical_file_system(epocver ver, const std::string &product_code)
            : ver(ver)
            , firmcode(product_code)
            , watcher_(nullptr) {
            for (auto &[drv, mapped] : mappings) {
                mapped = false;
            }
        }

        void set_epoc_ver(const epocver ever) override {
            ver = ever;
        }

        std::optional<std::u16string> get_raw_path(const std::u16string &path) override {
            return get_real_physical_path(path);
        }

        bool delete_entry(const std::u16string &path) override {
            std::optional<std::u16string> path_real = get_real_physical_path(path);

            if (!path_real) {
                return false;
            }

            return common::remove(common::ucs2_to_utf8(*path_real));
        }

        void set_product_code(const std::string &pc) override {
            firmcode = pc;
        }

        bool exists(const std::u16string &path) override {
            std::optional<std::u16string> real_path = get_real_physical_path(path);
            return real_path ? eka2l1::exists(common::ucs2_to_utf8(*real_path)) : false;
        }

        bool replace(const std::u16string &old_path, const std::u16string &new_path) override {
            std::optional<std::u16string> old_path_real = get_real_physical_path(old_path);
            std::optional<std::u16string> new_path_real = get_real_physical_path(new_path);

            if (!old_path_real || !new_path_real) {
                return false;
            }

            return common::move_file(common::ucs2_to_utf8(*old_path_real),
                common::ucs2_to_utf8(*new_path_real));
        }

        bool create_directories(const std::u16string &path) override {
            std::optional<std::u16string> real_path = get_real_physical_path(path);

            if (!real_path) {
                return false;
            }

            eka2l1::create_directories(common::ucs2_to_utf8(*real_path));
            return true;
        }

        bool create_directory(const std::u16string &path) override {
            std::optional<std::u16string> real_path = get_real_physical_path(path);

            if (!real_path) {
                return false;
            }

            eka2l1::create_directory(common::ucs2_to_utf8(*real_path));

            return true;
        }

        bool mount_volume_from_path(const drive_number drv, const drive_media media, const std::uint32_t attrib,
            const std::u16string &physical_path) override {
            if (media == drive_media::rom) {
                return false;
            }

            return do_mount(drv, media, attrib, physical_path);
        }

        bool unmount(const drive_number drv) override {
            if (mappings[static_cast<int>(drv)].second) {
                mappings[static_cast<int>(drv)].second = false;

                for (auto &watch_handle: watches[drv]) {
                    if (watcher_) {
                        watcher_->unwatch(static_cast<std::int32_t>(watch_handle));
                    }
                }

                watches[drv].clear();
                return true;
            }

            return false;
        }

        std::optional<drive> get_drive_entry(const drive_number drv) override {
            if (!mappings[static_cast<int>(drv)].second) {
                return std::nullopt;
            }

            return mappings[static_cast<int>(drv)].first;
        }

        std::unique_ptr<directory> open_directory(const std::u16string &path, const std::uint32_t attrib) override {
            std::u16string vir_path = path;

            size_t pos_bs = vir_path.find_last_of(u"\\");
            size_t pos_fs = vir_path.find_last_of(u"//");

            size_t pos_check = std::string::npos;

            if (pos_bs != std::string::npos && pos_fs != std::string::npos) {
                pos_check = std::max(pos_bs, pos_fs);
            } else if (pos_bs != std::string::npos) {
                pos_check = pos_bs;
            } else if (pos_fs != std::string::npos) {
                pos_check = pos_fs;
            }

            std::string filter("*");

            // Check if there should be a filter
            if (pos_check != std::string::npos && pos_check != vir_path.length() - 1) {
                // Substring this, get the filter
                filter = common::ucs2_to_utf8(vir_path.substr(pos_check + 1, vir_path.length() - pos_check - 1));
                vir_path.erase(vir_path.begin() + pos_check + 1, vir_path.end());
            }

            auto new_path = get_real_physical_path(vir_path);

            if (!new_path) {
                return std::unique_ptr<directory>(nullptr);
            }

            std::string new_path_utf8 = common::ucs2_to_utf8(*new_path);

            if (!eka2l1::exists(new_path_utf8)) {
                return std::unique_ptr<directory>(nullptr);
            }

            return std::make_unique<physical_directory>(this, new_path_utf8,
                common::ucs2_to_utf8(vir_path), filter, attrib);
        }

        std::optional<entry_info> get_entry_info(const std::u16string &path) override {
            std::optional<std::u16string> real_path = get_real_physical_path(path);

            if (!real_path) {
                return std::nullopt;
            }

            std::string real_path_utf8 = common::ucs2_to_utf8(*real_path);

            if (!eka2l1::exists(real_path_utf8)) {
                return std::nullopt;
            }

            entry_info info;

            if (common::is_file(real_path_utf8, common::FILE_DIRECTORY)) {
                info.type = io_component_type::dir;
                info.size = 0;
            } else {
                info.type = io_component_type::file;
                info.size = common::file_size(real_path_utf8);
            }

            /* TODO: Recover this code with new EKA2L1's common code.
            auto last_mod = fs::last_write_time(*real_path);
            info.last_write = static_cast<uint64_t>(last_mod.time_since_epoch().count());
            */

            std::string path_utf8 = common::ucs2_to_utf8(path);

            info.full_path = path_utf8;
            info.name = eka2l1::filename(path_utf8);

            const std::string root = eka2l1::root_name(path_utf8);
            drive &drv = mappings[ascii_to_drive_number(static_cast<char>(std::towlower(root[0])))].first;

            info.attribute = drv.attribute;

            return info;
        }

        std::unique_ptr<file> open_file(const std::u16string &path, const int mode) override {
            std::optional<std::u16string> real_path = get_real_physical_path(path);

            if (!real_path) {
                return nullptr;
            }

            std::string real_path_utf8 = common::ucs2_to_utf8(*real_path);

            if (!(mode & WRITE_MODE) && (!eka2l1::exists(real_path_utf8) || common::is_file(real_path_utf8, common::FILE_DIRECTORY))) {
                return nullptr;
            }

            return std::make_unique<physical_file>(path, *real_path, mode);
        }

        std::int64_t watch_directory(const std::u16string &path, common::directory_watcher_callback callback,
            void *callback_userdata, const std::uint32_t filters) override {
            const std::optional<std::u16string> real_path = get_raw_path(path);

            if (!real_path.has_value()) {
                return -1;
            }

            const std::u16string root = common::lowercase_ucs2_string(eka2l1::root_name(path, true));
            const drive_number drv = char16_to_drive(root[0]);

            if (!watcher_) {
                watcher_ = std::make_unique<common::directory_watcher>();
            }

            const std::int64_t handle = watcher_->watch(common::ucs2_to_utf8(real_path.value()), callback, callback_userdata, filters);
            if (handle < 0) {
                return handle;
            }

            watches[drv].push_back(handle);
            return handle;
        }

        bool unwatch_directory(const std::int64_t handle) override {
            if (!watcher_) {
                return false;
            }

            if (watcher_->unwatch(static_cast<std::int32_t>(handle))) {
                for (auto &[drive, watch_array]: watches) {
                    auto ite = std::find(watch_array.begin(), watch_array.end(), handle);
                    if (ite != watch_array.end()) {
                        watch_array.erase(ite);
                    }
                }

                return true;
            }

            return false;
        }
        
        void validate_for_host() override {
           if (common::is_platform_case_sensitive()) {
                LOG_INFO("Iterating through all emulated drive to lowercase all filesystem entities!");

                for (auto &mapping: mappings) {
                    if (!mapping.second) {
                        continue;
                    }

                    common::copy_folder(mapping.first.real_path, mapping.first.real_path, common::FOLDER_COPY_FLAG_LOWERCASE_NAME,
                        nullptr);
                }
            }
        }
    };

    class rom_file_system : public physical_file_system {
        loader::rom *rom_cache;
        memory_system *mem;

        loader::rom_dir *burn_tree_find_dir(const std::string &vir_path) {
            auto ite = path_iterator(vir_path);
            loader::rom_dir *last_dir_found = &(rom_cache->root.root_dirs[0].dir);

            // Skip through the drive
            ite++;

            std::vector<std::string> components;

            for (; ite; ite++) {
                components.push_back(*ite);
            }

            if (components.size() == 0) {
                return nullptr;
            }

            for (std::size_t i = 0; i < components.size(); i++) {
                loader::rom_dir temp;
                temp.name = common::utf8_to_ucs2(components[i]);

                auto res1 = std::lower_bound(last_dir_found->subdirs.begin(), last_dir_found->subdirs.end(), temp,
                    [](const loader::rom_dir &lhs, const loader::rom_dir &rhs) { return common::compare_ignore_case(lhs.name, rhs.name) == -1; });

                if (res1 != last_dir_found->subdirs.end() && (common::compare_ignore_case(res1->name, temp.name) == 0)) {
                    last_dir_found = &(last_dir_found->subdirs[std::distance(last_dir_found->subdirs.begin(), res1)]);
                } else {
                    return nullptr;
                }
            }

            return last_dir_found;
        }

        std::optional<loader::rom_entry> burn_tree_find_entry(const std::string &vir_path) {
            loader::rom_dir *last_dir_found = burn_tree_find_dir(eka2l1::file_directory(vir_path, true));

            if (!last_dir_found) {
                return std::nullopt;
            }

            loader::rom_entry temp_entry;
            temp_entry.name = common::utf8_to_ucs2(eka2l1::filename(vir_path, true));

            auto res2 = std::lower_bound(last_dir_found->entries.begin(), last_dir_found->entries.end(), temp_entry,
                [](const loader::rom_entry &lhs, const loader::rom_entry &rhs) { return common::compare_ignore_case(lhs.name, rhs.name) == -1; });

            if (res2 != last_dir_found->entries.end() && !res2->dir && (common::compare_ignore_case(temp_entry.name, res2->name) == 0)) {
                return *res2;
            }

            return std::nullopt;
        }

    public:
        explicit rom_file_system(loader::rom *cache, memory_system *mem, epocver ver, const std::string &product_code)
            : physical_file_system(ver, product_code)
            , rom_cache(cache)
            , mem(mem) {
        }

        bool delete_entry(const std::u16string &path) override {
            return false;
        }

        bool create_directories(const std::u16string &path) override {
            return false;
        }

        bool create_directory(const std::u16string &path) override {
            return false;
        }

        bool replace(const std::u16string &old_path, const std::u16string &new_path) override {
            return false;
        }

        bool mount_volume_from_path(const drive_number drv, const drive_media media, const std::uint32_t attrib,
            const std::u16string &physical_path) override {
            if (media != drive_media::rom) {
                return false;
            }

            return do_mount(drv, media, attrib, physical_path);
        }

        abstract_file_system_err_code is_entry_in_rom(const std::u16string &path) override {
            // Don't bother getting an entry if it's not even available on host
            if (!exists(path)) {
                return abstract_file_system_err_code::no;
            }

            if (burn_tree_find_entry(common::ucs2_to_utf8(path))) {
                return abstract_file_system_err_code::ok;
            }

            return abstract_file_system_err_code::no;
        }

        std::unique_ptr<file> open_file(const std::u16string &path, const int mode) override {
            if (mode & WRITE_MODE) {
                LOG_ERROR("Opening a read-only file (ROM + ROFS) with write mode");
                return nullptr;
            }

            // Don't bother getting an entry if it's not even available on host
            if (!exists(path)) {
                return nullptr;
            }

            std::u16string new_path = path;

            if (static_cast<int>(ver) > static_cast<int>(epocver::epoc6)) {
                if (common::compare_ignore_case(u"\\system\\libs", new_path.substr(2, 12)) == 0) {
                    new_path.replace(2, 12, u"\\sys\\bin");
                } else if (common::compare_ignore_case(u"\\system\\programs", new_path.substr(2, 16)) == 0) {
                    new_path.replace(2, 16, u"\\sys\\bin");
                }
            }

            auto entry = burn_tree_find_entry(common::ucs2_to_utf8(new_path));

            if (!entry) {
                return physical_file_system::open_file(new_path, mode);
            }

            return std::make_unique<rom_file>(mem, rom_cache, *entry);
        }

        std::optional<entry_info> get_entry_info(const std::u16string &path) override {
            // Don't bother getting an entry if it's not even available on host
            if (!exists(path)) {
                return std::nullopt;
            }

            auto entry = burn_tree_find_entry(common::ucs2_to_utf8(path));

            if (!entry) {
                return physical_file_system::get_entry_info(path);
            }

            entry_info info;
            info.type = entry->attrib & 0x10 ? io_component_type::dir : io_component_type::drive;
            info.has_raw_attribute = true;
            info.raw_attribute = entry->attrib;
            info.size = entry->size;
            info.name = common::ucs2_to_utf8(entry->name);
            info.full_path = common::ucs2_to_utf8(path);

            return info;
        }

        std::optional<std::u16string> find_entry_with_address(const std::u16string &clue, const address addr) override {
            std::u16string the_base_path = clue;
            loader::rom_dir *the_base_dir = &(rom_cache->root.root_dirs[0].dir);

            if (!the_base_path.empty()) {
                the_base_dir = burn_tree_find_dir(common::ucs2_to_utf8(clue));
            }

            if (!the_base_dir) {
                return std::nullopt;
            }

            for (const auto &entry: the_base_dir->entries) {
                if (entry.address_lin == addr) {
                    return entry.name;
                }
            }

            return std::nullopt;
        }
    };

    std::shared_ptr<abstract_file_system> create_physical_filesystem(const epocver ver, const std::string &product_code) {
        return std::make_unique<physical_file_system>(ver, product_code);
    }

    std::shared_ptr<abstract_file_system> create_rom_filesystem(loader::rom *rom_cache, memory_system *mem,
        const epocver ver, const std::string &product_code) {
        return std::make_unique<rom_file_system>(rom_cache, mem, ver, product_code);
    }

    io_component::io_component(io_component_type type, const std::uint32_t attrib)
        : type(type)
        , attribute(attrib) {
    }

    bool io_drive_callback_free_check_func(drive_change_callback_and_data &elem) {
        return !elem.first;
    }

    void io_drive_callback_free_func(drive_change_callback_and_data &elem) {
        elem.first = nullptr;
    }

    io_system::io_system()
        : drive_change_callbacks(io_drive_callback_free_check_func, io_drive_callback_free_func) {
    }

    io_system::~io_system() {
        filesystems.clear();
    }

    std::optional<filesystem_id> io_system::add_filesystem(file_system_inst &inst) {
        const std::lock_guard<std::mutex> guard(access_lock);

        ++id_counter;

        filesystems.emplace(id_counter, inst);
        return id_counter;
    }

    /*! \brief Remove the filesystem from the IO system
    */
    bool io_system::remove_filesystem(const filesystem_id id) {
        const std::lock_guard<std::mutex> guard(access_lock);

        if (id > id_counter) {
            return false;
        }

        filesystems.erase(id);
        return true;
    }

    bool io_system::mount_physical_path(const drive_number drv, const drive_media media, const std::uint32_t attrib,
        const std::u16string &real_path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, file_system] : filesystems) {
            if (file_system->mount_volume_from_path(drv, media, attrib, real_path)) {
                invoke_drive_change_callbacks(drv, drive_action_mount);
                return true;
            }
        }

        return false;
    }

    bool io_system::unmount(const drive_number drv) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, file_system] : filesystems) {
            if (file_system->unmount(drv)) {
                invoke_drive_change_callbacks(drv, drive_action_unmount);
                return true;
            }
        }

        return false;
    }

    std::optional<drive> io_system::get_drive_entry(const drive_number drv) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (auto entry = fs->get_drive_entry(drv)) {
                return entry;
            }
        }

        return std::nullopt;
    }

    std::unique_ptr<file> io_system::open_file(utf16_str vir_path, int mode) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (auto f = fs->open_file(vir_path, mode)) {
                return f;
            }
        }

        return nullptr;
    }

    std::unique_ptr<directory> io_system::open_dir(std::u16string vir_path, const std::uint32_t attrib) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (auto dir = fs->open_directory(vir_path, attrib)) {
                return dir;
            }
        }

        return nullptr;
    }

    bool io_system::exist(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (fs->exists(path)) {
                return true;
            }
        }

        return false;
    }

    bool io_system::rename(const std::u16string &old_path, const std::u16string &new_path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (fs->replace(old_path, new_path)) {
                return true;
            }
        }

        return false;
    }

    bool io_system::delete_entry(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (fs->delete_entry(path)) {
                return true;
            }
        }

        return false;
    }

    bool io_system::is_entry_in_rom(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (fs->is_entry_in_rom(path) == abstract_file_system_err_code::ok) {
                return true;
            }
        }

        return false;
    }

    bool io_system::create_directories(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (fs->create_directories(path)) {
                return true;
            }
        }

        return false;
    }

    bool io_system::create_directory(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (fs->create_directory(path)) {
                return true;
            }
        }

        return false;
    }

    std::optional<entry_info> io_system::get_entry_info(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (auto e = fs->get_entry_info(path)) {
                return e;
            }
        }

        return std::nullopt;
    }

    std::optional<std::u16string> io_system::find_entry_with_address(const std::u16string &clue, const address addr) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (auto e = fs->find_entry_with_address(clue, addr)) {
                return e;
            }
        }

        return std::nullopt;
    }
    
    std::optional<std::u16string> io_system::get_raw_path(const std::u16string &path) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            if (auto p = fs->get_raw_path(path)) {
                return p;
            }
        }

        return std::nullopt;
    }

    bool io_system::is_directory(const std::u16string &path) {
        std::optional<entry_info> ent = get_entry_info(path);

        if (!ent) {
            return false;
        }

        return ent->type == io_component_type::dir;
    }

    void io_system::set_product_code(const std::string &pc) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            fs->set_product_code(pc);
        }
    }

    void io_system::set_epoc_ver(const epocver ver) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            fs->set_epoc_ver(ver);
        }
    }

    std::int64_t io_system::watch_directory(const std::u16string &path, common::directory_watcher_callback callback,
        void *callback_userdata, const std::uint32_t filters) {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &[id, fs] : filesystems) {
            const std::int64_t result = fs->watch_directory(path, callback, callback_userdata, filters);

            if (result != -1) {
                return result | (id << 32);
            }
        }

        return -1;
    }

    bool io_system::unwatch_directory(const std::int64_t handle) {
        const std::lock_guard<std::mutex> guard(access_lock);

        const std::int32_t fs_id = static_cast<std::int32_t>(handle >> 32);
        auto fs_pair = filesystems.find(fs_id);

        if (fs_pair == filesystems.end()) {
            return false;
        }

        return fs_pair->second->unwatch_directory(handle);
    }

    void io_system::validate_for_host() {
        const std::lock_guard<std::mutex> guard(access_lock);

        for (auto &filesystem: filesystems) {
            filesystem.second->validate_for_host();
        }
    }

    std::size_t io_system::register_drive_change_notify(drive_change_notify_callback callback, void *userdata) {
        const std::lock_guard<std::mutex> guard(access_lock);
        
        auto pdat = std::make_pair(callback, userdata);
        return drive_change_callbacks.add(pdat);
    }
    
    void io_system::invoke_drive_change_callbacks(drive_number drv, drive_action act) {
        for (auto &callback: drive_change_callbacks) {
            access_lock.unlock();
            callback.first(callback.second, drv, act);
            access_lock.lock();
        }
    }

    symfile physical_file_proxy(const std::string &path, int mode) {
        return std::make_unique<physical_file>(common::utf8_to_ucs2(path), common::utf8_to_ucs2(path), mode);
    }

    void ro_file_stream::seek(const std::int64_t amount, common::seek_where wh) {
        f_->seek(amount, static_cast<file_seek_mode>(wh));
    }

    bool ro_file_stream::valid() {
        return f_->tell() < f_->size();
    }

    std::uint64_t ro_file_stream::left() {
        if (f_->tell() >= f_->size()) {
            return 0;
        }

        return f_->size() - f_->tell();
    }

    std::uint64_t ro_file_stream::tell() const {
        return f_->tell();
    }

    std::uint64_t ro_file_stream::size() {
        return f_->size();
    }

    std::uint64_t ro_file_stream::read(void *buf, const std::uint64_t read_size) {
        std::size_t result = f_->read_file(buf, static_cast<std::uint32_t>(read_size), 1);
        if (result == static_cast<std::size_t>(-1)) {
            return 0;
        }

        return result;
    }

    void wo_file_stream::seek(const std::int64_t amount, common::seek_where wh) {
        f_->seek(amount, static_cast<file_seek_mode>(wh));
    }

    bool wo_file_stream::valid() {
        return f_->valid();
    }

    std::uint64_t wo_file_stream::left() {
        return f_->size() - f_->tell();
    }

    std::uint64_t wo_file_stream::tell() const {
        return f_->tell();
    }

    std::uint64_t wo_file_stream::size() {
        return f_->size();
    }

    std::uint64_t wo_file_stream::write(const void *buf, const std::uint64_t write_size) {
        return f_->write_file(buf, static_cast<std::uint32_t>(write_size), 1);
    }
}
