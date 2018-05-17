#include <common/log.h>
#include <common/path.h>
#include <core/vfs.h>

#include <iostream>

#include <map>
#include <mutex>
#include <thread>

namespace eka2l1 {
    struct rom_file: public file {
        loader::rom_entry file;
        loader::rom* parent;

        uint64_t crr_pos;

        std::mutex mut;

        void init() {

        }

        uint64_t file_size() const override {
            return file.size;
        }

        int read_file(void* data, uint32_t size, uint32_t count) override {
            return 0;
        }

        int write_file(void* data, uint32_t size, uint32_t count) override {
            LOG_ERROR("Can't write into ROM!");
            return -1;
        }
    }

    struct physical_file: public file {
        FILE* file;

        uint64_t file_size() const override {
            auto crr_pos = ftell(file);
            fseek(file, 0, SEEK_END);

            auto res = ftell(file);
            fseek(file, crr_pos, SEEK_SET);

            return res;
        }

        bool close() override {
            fclose(file);
        }
    }

    void io_system::init() {
        _current_dir = "C:";
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

        if (res == _mount_map.end() || res.second.is_in_mem) {
            //log_error("Could not find partition");
            return "";
        }

        current_dir = res->second + crr_dir.substr(2);

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

    std::shared_ptr<file> io_system::open_file(std::string vir_path, int mode) {

    }
}
