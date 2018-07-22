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
    class memory_system;

    namespace loader {
        struct rom;
        struct rom_entry;
    }

    /*! \brief The seek mode of the file. */
    enum class file_seek_mode {
        //! Seeking from the beginning of the file.
        /*! This set the seek cursor to the offset.*/
        beg,

        //! Seeking from the current seek cursor
        /*! This set the seek cursor to be the current position of 
         * the seek cursor, plus with the offset provided.
         */
        crr,

        //! Seeking from the end of the file
        /*! This set the seek cursor to be the end of the file, plus
         * the provided offset.
         */
        end
    };

#define READ_MODE 0x100
#define WRITE_MODE 0x200
#define APPEND_MODE 0x300
#define BIN_MODE 0x400

    /*! \brief The file abstraction for VFS class
     *
     * This class contains all virtual method for virtual file (ROM file
     * and actual physical file).
    */
    struct file {
        /*! \brief Write to the file. 
         *
         * Write binary data to a file open with WRITE_MODE
         * 
         * \param data Pointer to the binary data.
         * \param size The size of each element the binary data
         * \param count Total element count.
         *
         * \returns Total bytes wrote. -1 if fail.
         */
        virtual size_t write_file(void *data, uint32_t size, uint32_t count) = 0;

        /*! \brief Read from the file. 
         *
         * Read from the file to the specified pointer
         * 
         * \param data Pointer to the destination.
         * \param size The size of each element to read
         * \param count Total element count.
         *
         * \returns Total bytes read. -1 if fail.
         */
        virtual size_t read_file(void *data, uint32_t size, uint32_t count) = 0;

        /*! \brief Get the file mode which specified with VFS system. 
         * \returns The file mode.
         */
        virtual int file_mode() const = 0;

        /*! \brief Get the full path of the file. 
         * \returns The full path of the file.
         */
        virtual std::u16string file_name() const = 0;

        /*! \brief Size of the file. 
         * \returns The size of the file.
        */
        virtual uint64_t size() const = 0;

        /*! \brief Seek the file with specified mode. 
         */
        virtual void seek(int seek_off, file_seek_mode where) = 0;

        /*! \brief Get the position of the seek cursor.
         * \returns The position of the seek cursor.
         */
        virtual uint64_t tell() = 0;

        /*! \brief Close the file. 
         * \returns True if file is closed successfully.
         */
        virtual bool close() = 0;

        /*! \brief Please don't use this. */
        virtual std::string get_error_descriptor() = 0;
    };

    using symfile = std::shared_ptr<file>;

    /*! \brief A VFS drive. */
    struct drive {
        //! Attribute that true if the drive is in memory
        /*! Some drives are in the memory. This included Z: (ROM region)
         * and D: (small RAM region).
        */
        bool is_in_mem;

        //! The name of the drive.
        /*! Lowercase of uppercase (c: or C:) doesn't matter. Like Windows,
         * path in Symbian are insensitive.
        */
        std::string drive_name;

        //! The physical path of the drive.
        /*! This exists if the drive is physically available
        */
        std::string real_path;
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

        epocver ver;

    public:
        // Initialize the IO system
        void init(memory_system *smem, epocver ever);

        void set_epoc_version(epocver ever) {
            ver = ever;
        }

        // Shutdown the IO system
        void shutdown();

        // Returns the current directory of the system
        std::string current_dir();

        // Set the current directory
        void current_dir(const std::string &new_dir);

        // Mount a physical path to a device
        void mount(const std::string &dvc, const std::string &real_path, bool in_mem = false);

        // Unmount a device
        void unmount(const std::string &dvc);

        // Map a virtual path to real path. Return "" if this can't be mapped to real
        std::string get(std::string vir_path);

        // Open a file. Return is a shared pointer of the file interface.
        std::shared_ptr<file> open_file(std::u16string vir_path, int mode);
    };
}
