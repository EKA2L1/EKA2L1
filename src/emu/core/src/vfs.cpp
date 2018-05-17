#include <common/log.h>
#include <common/path.h>
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

        void init() {
            file_ptr = ptr<char>(file.address_lin);
        }

        uint64_t size() const override {
            return file.size;
        }

        int read_file(void* data, uint32_t size, uint32_t count) override {
            auto will_read = std::min((uint64_t)count, file.size - crr_pos);
            memcpy(data, &file_ptr.get(mem)[crr_pos], will_read);

            crr_pos += will_read;

            return will_read;
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

        void init(std::string inp_name) {
            input_name = std::u16string(inp_name.begin(), inp_name.end());
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

    std::shared_ptr<file> io_system::open_file(std::string vir_path, int mode) {
        return std::shared_ptr<file>(nullptr);
    }
}
