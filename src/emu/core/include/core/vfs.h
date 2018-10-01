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

#include <common/types.h>

#include <array>
#include <map>
#include <memory>
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
        end,

        /**! Same as seeking from begging, but this indicates that 
         * the return value must be a linear address. Will return 0xFFFFFFFF
         * if the file is not ROM
         */
        address
    };

#define READ_MODE 0x100
#define WRITE_MODE 0x200
#define APPEND_MODE 0x300
#define BIN_MODE 0x400

    enum class io_component_type {
        file,
        dir,
        drive
    };

    enum class io_attrib {
        none,
        include_dir = 0x50,
        hidden = 0x100,
        write_protected = 0x200,
        internal = 0x400,
        removeable = 0x800
    };

    inline io_attrib operator|(io_attrib a, io_attrib b) {
        return static_cast<io_attrib>(static_cast<int>(a) | static_cast<int>(b));
    }

    inline io_attrib operator&(io_attrib a, io_attrib b) {
        return static_cast<io_attrib>(static_cast<int>(a) & static_cast<int>(b));
    }

    struct io_component {
        io_attrib attribute;
        io_component_type type;

        io_component() {}
        virtual ~io_component() {}

        explicit io_component(io_component_type type, io_attrib attrib = io_attrib::none);
    };

    /*! \brief The file abstraction for VFS class
     *
     * This class contains all virtual method for virtual file (ROM file
     * and actual physical file).
    */
    struct file : public io_component {
        explicit file(io_attrib attrib = io_attrib::none);

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
        virtual size_t seek(size_t seek_off, file_seek_mode where) = 0;

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

        /*! \brief Check if the file is in ROM or not. 
         *
         * Notice that files in Z: drive is not always ROM file. Z: drive is a combination 
         * of ROFS and ROM (please see EBootMagic.txt in sys/data).
         */
        virtual bool is_in_rom() const = 0;

        /*! \brief Get the address of the file in the ROM. 
        *
        * If the file is not in ROM, this return a null pointer.
        */
        virtual address rom_address() const = 0;

        virtual bool flush();
    };

    using symfile = std::shared_ptr<file>;
    using io_component_ptr = std::shared_ptr<io_component>;

    enum class drive_media {
        none,
        usb,
        physical,
        reflect,
        rom,
        ram
    };

    /*! \brief A VFS drive. */
    struct drive : public io_component {
        explicit drive(io_attrib attrib = io_attrib::none);

        //! The name of the drive.
        /*! Lowercase of uppercase (c: or C:) doesn't matter. Like Windows,
        * path in Symbian are insensitive.
        */
        std::string drive_name;

        //! The physical path of the drive.
        /*! This exists if the drive is physically available
        */
        std::string real_path;

        //! The media type of the drive
        drive_media media_type;
    };

    struct entry_info {
        io_attrib attribute;

        std::string name;
        std::string full_path;

        io_component_type type;
        size_t size;
    };

    struct directory : public io_component {
        explicit directory(io_attrib attrib = io_attrib::none);

        /*! \brief Get the next iterating entry. 
        *
        * All the entries are filtered through a regex expression. The directory iterator
        * will increase itself if it's not at the end entry, and returns the entry info. Else,
        * it will return nothing
        */
        virtual std::optional<entry_info> get_next_entry() = 0;

        virtual std::optional<entry_info> peek_next_entry() = 0;
    };

    class io_system {
        std::map<std::string, symfile> file_caches;
        std::array<drive, drive_count> drives;

        loader::rom *rom_cache;

        std::string pref_path;

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

        void set_rom_cache(loader::rom *cache) {
            rom_cache = cache;
        }

        // Shutdown the IO system
        void shutdown();

        // Mount a physical path to a device
        void mount(const drive_number dvc, const drive_media media, const std::string &real_path,
            const io_attrib attrib = io_attrib::none);

        // Unmount a device
        void unmount(const drive_number dvc);

        // Map a virtual path to real path. Return "" if this can't be mapped to real
        std::string get(std::string vir_path);

        // Open a file. Return is a shared pointer of the file interface.
        std::shared_ptr<file> open_file(std::u16string vir_path, int mode);
        std::shared_ptr<directory> open_dir(std::u16string vir_path, const io_attrib attrib = io_attrib::none);

        drive get_drive_entry(drive_number drv);
    };

    symfile physical_file_proxy(const std::string &path, int mode);
}
