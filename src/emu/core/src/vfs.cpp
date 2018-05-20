#include <common/log.h>
#include <common/path.h>
#include <common/cvt.h>

#include <core/vfs.h>
#include <core/core_mem.h>
#include <core/loader/rom.h>
#include <core/ptr.h>

#include <iostream>

#include <map>
#include <mutex>
#include <thread>

namespace eka2l1 {
    // Class for some one want to access rom
    struct rom_file: public file {
        loader::rom_entry file;
        loader::rom* parent;

        uint64_t crr_pos;
        std::mutex mut;

        memory* mem;

        ptr<char> file_ptr;

        rom_file(memory* mem, loader::rom* supereme_mother, loader::rom_entry entry)
            : parent(supereme_mother), file(entry), mem(mem) { init(); }

        void init() {
            file_ptr = ptr<char>(file.address_lin);
        }

        uint64_t size() const override {
            return file.size;
        }

        int read_file(void* data, uint32_t size, uint32_t count) override {
            auto will_read = std::min((uint64_t)count * size, file.size - crr_pos);
            memcpy(data, &file_ptr.get(mem)[crr_pos], will_read);

            crr_pos += will_read;

            return will_read;
        }

        int file_mode() const override {
            return READ_MODE;
        }

        int write_file(void* data, uint32_t size, uint32_t count) override {
            LOG_ERROR("Can't write into ROM!");
            return -1;
        }

        void seek(uint32_t seek_off, file_seek_mode where) override {
            if (where == file_seek_mode::beg) {
                crr_pos = seek_off;
            } else if (where == file_seek_mode::crr) {
                crr_pos += seek_off;
            } else {
                crr_pos += size() + seek_off;
            }
        }

        std::u16string file_name() const override {
            return file.name;
        }

        bool close() override {
            return true;
        }
    };

    struct physical_file: public file {
        FILE* file;
        std::u16string input_name;
        int fmode;

        const char* translate_mode(int mode) {
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

        physical_file(utf16_str path, int mode)
            { init(path, mode); }

        ~physical_file() { shutdown(); }

        int file_mode() const override {
            return fmode;
        }

        void init(utf16_str inp_name, int mode) {
            file = fopen(common::ucs2_to_utf8(inp_name).c_str(), translate_mode(mode));
            input_name = std::u16string(inp_name.begin(), inp_name.end());
            fmode = mode;
        }

        void shutdown() {
            fclose(file);
        }
        
        int write_file(void* data, uint32_t size, uint32_t count) override {
            return fwrite(data, size, count, file);
        }
        
        int read_file(void* data, uint32_t size, uint32_t count) override {
            return fread(data, size, count, file);
        }

        uint64_t size() const override {
            auto crr_pos = ftell(file);
            fseek(file, 0, SEEK_END);

            auto res = ftell(file);
            fseek(file, crr_pos, SEEK_SET);

            return res;
        }

        bool close() override {
            fclose(file);
            return true;
        }

        void seek(uint32_t seek_off, file_seek_mode where) override {
            if (where == file_seek_mode::beg) {
                fseek(file, seek_off, SEEK_SET);
            } else if (where == file_seek_mode::crr) {
                fseek(file, seek_off, SEEK_CUR);
            } else {
                fseek(file, seek_off, SEEK_END);
            }
        }

        std::u16string file_name() const override {
            return input_name;
        }
    };

    void io_system::init(memory* smem) {
        mem = smem;
        crr_dir = "C:";
    }

    void io_system::shutdown() {
        drives.clear();
    }

    std::string io_system::current_dir() {
        return crr_dir;
    }

    void io_system::current_dir(const std::string &new_dir) {
        std::lock_guard<std::mutex> guard(mut);
        crr_dir = new_dir;
    }

    void io_system::mount(const std::string &dvc, const std::string &real_path) {
        auto find_res = drives.find(dvc);

        if (find_res == drives.end()) {
            // Warn
        }

        drive drv;
        drv.is_in_mem = false;
        drv.drive_name = dvc;
        drv.real_path = real_path;

        std::lock_guard<std::mutex> guard(mut);
        drives.insert(std::make_pair(dvc, drv));

        crr_dir = dvc;
    }

    void io_system::unmount(const std::string &dvc) {
        std::lock_guard<std::mutex> guard(mut);
        drives.erase(dvc);
    }

    std::string io_system::get(std::string vir_path) {
        std::string abs_path = "";

        std::string current_dir = crr_dir;

        // Current directory is always an absolute path
        std::string partition;

        if (vir_path.find_first_of(':') == 1) {
            partition = vir_path.substr(0, 2);
        } else {
            partition = current_dir.substr(0, 2);
        }

        partition[0] = std::toupper(partition[0]);

        auto res = drives.find(partition);

        if (res == drives.end() || res->second.is_in_mem) {
            //log_error("Could not find partition");
            return "";
        }

        current_dir = res->second.real_path + crr_dir.substr(2);

        // Make it case-insensitive
        for (auto &c : vir_path) {
            c = std::tolower(c);
        }

        if (!is_absolute(vir_path, current_dir)) {
            abs_path = absolute_path(vir_path, current_dir);
        } else {
            abs_path = add_path(res->second.real_path, vir_path.substr(2));
        }

        return abs_path;
    }

    std::optional<drive> io_system::find_dvc(std::string vir_path) {
        auto first_dvc_colon = vir_path.find_first_of(":");

        if (!(FOUND_STR(first_dvc_colon))) {
            return std::optional<drive>{};
        }

        std::string path_dvc = vir_path.substr(0, 3);

        auto findres = drives.find(path_dvc);

        if (findres != drives.end())  {
            return findres->second;
        }

        return std::optional<drive>{};
    }

    // Gurantees that these path are ASCII (ROM you says ;) )
    std::optional<loader::rom_entry> io_system::burn_tree_find_entry(const std::string& vir_path) {
        auto dirs = rom_cache->root[0].dir.subdirs;
        auto ite = path_iterator(vir_path);

        loader::rom_dir* last_dir_found; 

        ++ite;

        for (; ite; ++ite) {
            auto res1 = std::find_if(dirs.begin(), dirs.end(),
                [ite](auto p1) { return p1->name == utf16_str((*ite).begin(), (*ite).end()); });

            if (res1 != dirs.end()) {
                dirs = (*res1)->subdirs;
                last_dir_found = *res1;
            }

            return std::optional<loader::rom_entry>{};
         }

        // Save the last
        auto entries = last_dir_found->entries;

        auto res2 = std::find_if(entries.begin(), entries.end(),
            [ite](auto p2) { return p2.name == utf16_str((*ite).begin(), (*ite).end()); });

        if (res2 != entries.end() && !res2->dir) {
            return *res2;
        }

        return std::optional<loader::rom_entry>{};
    }

    // USC2 is not even supported yet, as this is retarded
    std::shared_ptr<file> io_system::open_file(utf16_str vir_path, int mode) {
        auto res = find_dvc(common::ucs2_to_utf8(vir_path));

        if (!res) {
            return std::shared_ptr<file>(nullptr);
        }

        drive drv = res.value();

        if (drv.is_in_mem && mode == WRITE_MODE) {
            auto rom_entry = burn_tree_find_entry(std::string(vir_path.begin(), vir_path.end()));
            
            if (!rom_entry) {
                return std::shared_ptr<file>(nullptr);
            }

            return std::make_shared<rom_file>(mem, rom_cache, rom_entry.value());
        } else {
            auto new_path = get(common::ucs2_to_utf8(vir_path));
            return std::make_shared<physical_file>(utf16_str(new_path.begin(), new_path.end()), mode);
        }

        return std::shared_ptr<file>(nullptr);
    }
}
