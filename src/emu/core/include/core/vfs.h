#pragma once

#include <string>

// VFS
namespace eka2l1 {
    // A file abstraction
    struct file {
        virtual int write_file(void* data, uint32_t size, uint32_t count) = 0;
        virtual int read_file(void* data, uint32_t size, uint32_t count) = 0;

        virtual int file_mode() const = 0;

        virtual std::u16string file_name() const = 0;

        virtual uint64_t size() const = 0;
        virtual void seek(uint32_t seek_off, uint32_t where) = 0;

        virtual bool close() = 0;
    }

    struct directory {
        virtual int file_count() const = 0;
        virtual std::u16string directory_name() const = 0;
    }

    struct drive {
        bool is_in_mem;

        std::string drive_name;
        std::string real_path; // This exists if the drive is physically available
    }

    class io_system {
        std::map<std::string, file> file_caches;
        std::map<std::string, drive> drives;

        loader::rom rom_cache;

        std::string pref_path;
        std::string crr_dir;

        std::mutex mut;

    public:
        // Initialize the IO system
        void init();

        // Shutdown the IO system
        void shutdown();

        // Returns the current directory of the system
        std::string current_dir();

        // Set the current directory
        void current_dir(const std::string &new_dir);

        // Mount a physical path to a device
        void mount(const std::string &dvc, const std::string &real_path);

        // Mount a ROM to a device. This is usually Z:
        void mount_rom(const std::string& dvc, const std::string& rom_path);

        // Unmount a device
        void unmount(const std::string &dvc);

        // Map a virtual path to real path. Return "" if this can't be mapped to real
        std::string get(std::string vir_path);

        // Open a file. Return is a shared pointer of the file interface.
        std::shared_ptr<file> open_file(std::string vir_path, int mode);
    }
}
