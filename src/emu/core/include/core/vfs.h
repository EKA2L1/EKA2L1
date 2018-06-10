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
#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>

// VFS
namespace eka2l1 {
    class memory;

    namespace loader {
        struct rom;
        struct rom_entry;
    }

    enum class file_seek_mode {
        beg,
        crr,
        end
    };

#define READ_MODE 0x100
#define WRITE_MODE 0x200
#define APPEND_MODE 0x300
#define BIN_MODE 0x400

    // A file abstraction
    struct file {
        virtual int write_file(void *data, uint32_t size, uint32_t count) = 0;
        virtual int read_file(void *data, uint32_t size, uint32_t count) = 0;

        virtual int file_mode() const = 0;

        virtual std::u16string file_name() const = 0;

        virtual uint64_t size() const = 0;

        virtual void seek(uint32_t seek_off, file_seek_mode where) = 0;
        virtual uint64_t tell() = 0;

        virtual bool close() = 0;

        virtual std::string get_error_descriptor() = 0;
    };

    using symfile = std::shared_ptr<file>;

    struct drive {
        bool is_in_mem;

        std::string drive_name;
        std::string real_path; // This exists if the drive is physically available
    };

    class io_system {
        std::map<std::string, symfile> file_caches;
        std::map<std::string, drive> drives;

        loader::rom *rom_cache;

        std::string pref_path;
        std::string crr_dir;

        std::mutex mut;

        memory_system *mem;

        std::optional<drive> find_dvc(std::string vir_path);

        // Burn the tree down and look for entry in the dust
        std::optional<loader::rom_entry> burn_tree_find_entry(const std::string &vir_path);

    public:
        // Initialize the IO system
        void init(memory_system *smem);

        // Shutdown the IO system
        void shutdown();

        // Returns the current directory of the system
        std::string current_dir();

        // Set the current directory
        void current_dir(const std::string &new_dir);

        // Mount a physical path to a device
        void mount(const std::string &dvc, const std::string &real_path);

        // Mount a ROM to a device. This is usually Z:
        void mount_rom(const std::string &dvc, loader::rom *rom);

        // Unmount a device
        void unmount(const std::string &dvc);

        // Map a virtual path to real path. Return "" if this can't be mapped to real
        std::string get(std::string vir_path);

        // Open a file. Return is a shared pointer of the file interface.
        std::shared_ptr<file> open_file(std::u16string vir_path, int mode);
    };
}

