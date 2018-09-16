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
#include <common/log.h>
#include <common/path.h>

#include <core/core_mem.h>
#include <core/loader/rom.h>
#include <core/ptr.h>
#include <core/vfs.h>

#include <experimental/filesystem>

#include <iostream>

#include <map>
#include <mutex>
#include <regex>
#include <thread>

namespace fs = std::experimental::filesystem;

namespace eka2l1 {
    file::file(io_attrib attrib)
        : io_component(io_component_type::file, attrib) {
    }

    drive::drive(io_attrib attrib)
        : io_component(io_component_type::drive, attrib) {
    }

    directory::directory(io_attrib attrib)
        : io_component(io_component_type::dir, attrib) {
    }

    // Class for some one want to access rom
    struct rom_file : public file {
        loader::rom_entry file;
        loader::rom *parent;

        uint64_t crr_pos;
        std::mutex mut;

        memory_system *mem;

        ptr<char> file_ptr;

        rom_file(memory_system *mem, loader::rom *supereme_mother, loader::rom_entry entry)
            : parent(supereme_mother)
            , file(entry)
            , mem(mem) { init(); }

        void init() {
            file_ptr = ptr<char>(file.address_lin);
            crr_pos = 0;
        }

        uint64_t size() const override {
            return file.size;
        }

        size_t read_file(void *data, uint32_t size, uint32_t count) override {
            auto will_read = std::min((uint64_t)count * size, file.size - crr_pos);
            memcpy(data, &(file_ptr.get(mem)[crr_pos]), will_read);

            crr_pos += will_read;

            return static_cast<int>(will_read);
        }

        int file_mode() const override {
            return READ_MODE;
        }

        size_t write_file(void *data, uint32_t size, uint32_t count) override {
            LOG_ERROR("Can't write into ROM!");
            return -1;
        }

        size_t seek(size_t seek_off, file_seek_mode where) override {
            if (where == file_seek_mode::beg || where == file_seek_mode::address) {
                crr_pos = seek_off;
            } else if (where == file_seek_mode::crr) {
                crr_pos += seek_off;
            } else {
                crr_pos += size() + seek_off;
            }

            if (where == file_seek_mode::address) {
                return file.address_lin + crr_pos;
            }

            return crr_pos;
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
    };

    struct physical_file : public file {
        FILE *file;
        std::u16string input_name;
        int fmode;

        size_t file_size;
        bool closed;

        const char *translate_mode(int mode) {
            if (mode & READ_MODE) {
                if (mode & BIN_MODE) {
                    if (mode & WRITE_MODE) {
                        return "rwb";
                    }

                    return "rb";
                } else if (mode & WRITE_MODE) {
                    return "rw";
                }

                return "r";
            } else if (mode & WRITE_MODE) {
                if (mode & BIN_MODE) {
                    return "wb";
                }

                return "w";
            } else if (mode & APPEND_MODE) {
                if (mode & BIN_MODE) {
                    return "ab";
                }

                return "a";
            }

            return "";
        }

#define WARN_CLOSE \
    if (closed)    \
        LOG_WARN("File {} closed but operation still continues", common::ucs2_to_utf8(input_name));

        physical_file(utf16_str path, int mode) { init(path, mode); }

        ~physical_file() {
            shutdown();
        }

        int file_mode() const override {
            return fmode;
        }

        void init(utf16_str inp_name, int mode) {
            closed = false;

            const char *cmode = translate_mode(mode);
            file = fopen(common::ucs2_to_utf8(inp_name).c_str(), cmode);
            input_name = std::move(inp_name);
            fmode = mode;

            if (file) {
                auto crr_pos = ftell(file);
                fseek(file, 0, SEEK_END);

                file_size = ftell(file);
                fseek(file, crr_pos, SEEK_SET);
            }
        }

        void shutdown() {
            if (file && !closed)
                fclose(file);
        }

        size_t write_file(void *data, uint32_t size, uint32_t count) override {
            WARN_CLOSE

            return fwrite(data, size, count, file);
        }

        size_t read_file(void *data, uint32_t size, uint32_t count) override {
            WARN_CLOSE

            return fread(data, size, count, file);
        }

        uint64_t size() const override {
            WARN_CLOSE

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

        size_t seek(size_t seek_off, file_seek_mode where) override {
            WARN_CLOSE

            if (where == file_seek_mode::address) {
                return 0xFFFFFFFF;
            }

            if (where == file_seek_mode::beg) {
                fseek(file, seek_off, SEEK_SET);
            } else if (where == file_seek_mode::crr) {
                fseek(file, seek_off, SEEK_CUR);
            } else {
                fseek(file, seek_off, SEEK_END);
            }

            return ftell(file);
        }

        std::u16string file_name() const override {
            WARN_CLOSE

            return input_name;
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

        fs::directory_iterator dir_iterator;

        std::optional<entry_info> peek_info;
        bool peeking;

    protected:
        std::string replace_all(std::string str, const std::string &from, const std::string &to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
            }
            return str;
        }

        std::string construct_regex_string(std::string regexstr) {
            regexstr = replace_all(regexstr, "\\", "\\\\");
            regexstr = replace_all(regexstr, ".", std::string("\\") + ".");
            regexstr = replace_all(regexstr, "?", ".");
            regexstr = replace_all(regexstr, "*", ".*");

            return regexstr;
        }

    public:
        physical_directory(const std::string &phys_path, const std::string &vir_path, const std::string &filter)
            : filter(construct_regex_string(filter))
            , dir_iterator(phys_path)
            , vir_path(vir_path)
            , peeking(false) {
        }

        std::optional<entry_info> get_next_entry() override {
            if (peeking) {
                peeking = false;
                return peek_info;
            }

            while (true) {
                std::error_code err;
                dir_iterator.increment(err);

                if (static_cast<bool>(err) || dir_iterator == fs::directory_iterator{}) {
                    return std::optional<entry_info>{};
                }

                std::string name = "";

                try {
                    name = dir_iterator->path().filename().string();
                }
                catch (...) {
                    return std::optional<entry_info>{};
                }

                // If it doesn't meet the filter, continue until find one or there is no one
                if (!std::regex_match(name, filter)) {
                    continue;
                }

                entry_info info;
                info.attribute = io_attrib::none;
                info.full_path = eka2l1::add_path(vir_path, name);
                info.name = name;
                info.type = dir_iterator->status().type() == fs::file_type::regular ? io_component_type::file
                                                                                    : io_component_type::dir;
                info.size = dir_iterator->status().type() == fs::file_type::regular ? fs::file_size(dir_iterator->path())
                                                                                    : 0;

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

    io_component::io_component(io_component_type type, io_attrib attrib)
        : type(type)
        , attribute(attrib) {
    }

    void io_system::init(memory_system *smem, epocver ever) {
        mem = smem;
        ver = ever;

        drive drvt;
        drvt.media_type = drive_media::none;

        std::fill(drives.begin(), drives.end(), drvt);
    }

    void io_system::shutdown() {
        if (mem) {
            file_caches.clear();
        }
    }

    void io_system::mount(const drive_number drv, const drive_media media, const std::string &real_path, const io_attrib attrib) {
        std::lock_guard<std::mutex> guard(mut);

        drive &drvm = drives[drv];

        if (drvm.media_type != drive_media::none) {
            return;
        }

        char drive_dos_char = char(0x41 + drv);

        drvm.media_type = media;
        drvm.drive_name = std::string(&drive_dos_char, 1) + ":";
        drvm.real_path = real_path;
        drvm.attribute = attrib;
    }

    void io_system::unmount(const drive_number drv) {
        std::lock_guard<std::mutex> guard(mut);

        drives[drv].media_type = drive_media::none;
    }

    std::string io_system::get(std::string vir_path) {
        std::string abs_path = "";

        // Current directory is always an absolute path
        std::string partition;

        if (vir_path.find_first_of(':') == 1) {
            partition = vir_path.substr(0, 2);
        } else {
            return "";
        }

        auto res = std::find_if(drives.begin(), drives.end(),
            [&](drive &drv) { return drv.drive_name == partition; });

        if (res == drives.end() || (res->media_type == drive_media::rom)) {
            partition[0] = std::toupper(partition[0]);

            res = std::find_if(drives.begin(), drives.end(),
                [&](drive &drv) { return drv.drive_name == partition; });

            if (res == drives.end()) {
                return "";
            }
        }

        std::string rp = res->real_path;

        if (res->media_type == drive_media::rom) {
            switch (ver) {
            case epocver::epoc93: {
                rp = add_path(rp, "v93\\");
                break;
            }

            case epocver::epoc9: {
                rp = add_path(rp, "v94\\");
                break;
            }

            case epocver::epoc10: {
                rp = add_path(rp, "belle\\");
                break;
            }

            case epocver::epoc6: {
                rp = add_path(rp, "v60\\");
                break;
            }

            default:
                break;
            }
        }

        // Make it case-insensitive
        for (auto &c : vir_path) {
            c = std::tolower(c);
        }

        size_t lib_pos = vir_path.find("\\system\\lib");

        // TODO (bentokun): Remove this hack with a proper symlink system.
        if (lib_pos != std::string::npos && static_cast<int>(ver) > static_cast<int>(epocver::epoc6)) {
            vir_path.replace(lib_pos, 12, "\\sys\\bin");
        }

        abs_path = add_path(rp, vir_path.substr(2));

        return abs_path;
    }

    std::optional<drive> io_system::find_dvc(std::string vir_path) {
        auto first_dvc_colon = vir_path.find_first_of(":");

        if (!(FOUND_STR(first_dvc_colon))) {
            return std::optional<drive>{};
        }

        std::string path_dvc = vir_path.substr(0, 2);

        auto findres = std::find_if(drives.begin(), drives.end(),
            [&](drive &drv) { return drv.drive_name == path_dvc; });

        if (findres != drives.end()) {
            return *findres;
        } else {
            if (std::islower(path_dvc[0]))
                path_dvc[0] = std::toupper(path_dvc[0]);
            else
                path_dvc[0] = std::tolower(path_dvc[0]);

            findres = std::find_if(drives.begin(), drives.end(),
                [&](drive &drv) { return drv.drive_name == path_dvc; });

            if (findres != drives.end()) {
                return *findres;
            }
        }

        return std::optional<drive>{};
    }

    // Gurantees that these path are ASCII (ROM you says ;) )
    // This is only invoked when user check search in ROM config if can't find needed file.
    std::optional<loader::rom_entry> io_system::burn_tree_find_entry(const std::string &vir_path) {
        std::vector<loader::rom_dir> dirs = rom_cache->root.root_dirs[0].dir.subdirs;
        auto ite = path_iterator(vir_path);

        loader::rom_dir last_dir_found;

        ++ite;

        for (; ite; ++ite) {
            loader::rom_dir temp;
            temp.name = common::utf8_to_ucs2(*ite);

            auto res1 = std::lower_bound(dirs.begin(), dirs.end(), temp,
                [](const loader::rom_dir &lhs, const loader::rom_dir &rhs) { return common::compare_ignore_case(lhs.name, rhs.name) == -1; });

            if (res1 != dirs.end() && (common::compare_ignore_case(res1->name, temp.name) == 0)) {
                last_dir_found = *res1;
                dirs = res1->subdirs;
            }
        }

        if (ite) {
            return std::optional<loader::rom_entry>{};
        }

        // Save the last
        auto entries = last_dir_found.entries;

        loader::rom_entry temp_entry;
        temp_entry.name = common::utf8_to_ucs2(*ite);

        auto res2 = std::lower_bound(entries.begin(), entries.end(), temp_entry,
            [](const loader::rom_entry &lhs, const loader::rom_entry &rhs) { return common::compare_ignore_case(lhs.name, rhs.name) == -1; });

        if (res2 != entries.end() && !res2->dir && (common::compare_ignore_case(temp_entry.name, res2->name) == 0)) {
            return *res2;
        }

        return std::optional<loader::rom_entry>{};
    }

    // USC2 is not even supported yet, as this is retarded
    std::shared_ptr<file> io_system::open_file(utf16_str vir_path, int mode) {
        std::string path_u8 = common::ucs2_to_utf8(vir_path);
        auto res = find_dvc(path_u8);

        if (!res) {
            return std::shared_ptr<file>(nullptr);
        }

        drive drv = res.value();

        if (drv.media_type == drive_media::rom) {
            if (mode & WRITE_MODE) {
                LOG_INFO("No writing in in-memory!");
                return std::shared_ptr<file>(nullptr);
            }

            std::optional<loader::rom_entry> entry = burn_tree_find_entry(path_u8);

            if (entry) {
                auto romf = std::make_shared<rom_file>(mem, rom_cache, *entry);

                if (romf) {
                    return romf;
                }
            }
        }

        auto new_path = get(path_u8);
        auto pf = std::make_shared<physical_file>(common::utf8_to_ucs2(new_path), mode);

        if (!pf->file) {
            return std::shared_ptr<file>(nullptr);
        }

        return pf;
    }

    std::shared_ptr<directory> io_system::open_dir(std::u16string vir_path) {
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

        auto res = find_dvc(common::ucs2_to_utf8(vir_path));

        if (!res) {
            return std::shared_ptr<directory>(nullptr);
        }

        drive drv = res.value();
        auto new_path = get(common::ucs2_to_utf8(vir_path));

        if (!fs::exists(new_path)) {
            return std::shared_ptr<directory>(nullptr);
        }

        return std::make_shared<physical_directory>(new_path, common::ucs2_to_utf8(vir_path), filter);
    }

    drive io_system::get_drive_entry(drive_number drv) {
        return drives[drv];
    }

    symfile physical_file_proxy(const std::string &path, int mode) {
        return std::make_shared<physical_file>(common::utf8_to_ucs2(path), mode);
    }
}
