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

#include <common/buffer.h>
#include <common/types.h>
#include <common/watcher.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

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

    struct io_component {
        io_attrib attribute;
        io_component_type type;

        io_component() {}
        virtual ~io_component() {
        }

        explicit io_component(io_component_type type, io_attrib attrib = io_attrib::none);
    };

    /*! \brief The file abstraction for VFS class
     *
     * This class contains all virtual method for virtual file (ROM file
     * and actual physical file).
    */
    struct file : public io_component {
        explicit file(io_attrib attrib = io_attrib::none);

        virtual ~file() override {
        }

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
        virtual size_t write_file(const void *data, uint32_t size, uint32_t count) = 0;

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
        virtual uint64_t seek(std::int64_t seek_off, file_seek_mode where) = 0;

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

        virtual bool resize(const std::size_t new_size) = 0;

        virtual bool flush();

        virtual bool valid() = 0;

        virtual std::uint64_t last_modify_since_1ad() = 0;

        std::size_t read_file(const std::uint64_t offset, void *buf, std::uint32_t size,
            std::uint32_t count);
    };

    using symfile = std::unique_ptr<file>;
    using io_component_ptr = std::unique_ptr<io_component>;

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
        int raw_attribute;

        bool has_raw_attribute = false;

        std::string name;
        std::string full_path;

        io_component_type type;
        std::size_t size;
        std::uint64_t last_write;
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

    enum class abstract_file_system_err_code {
        unsupported,
        failed,
        no,
        ok
    };

    /* \brief An abstract filesystem
    */
    class abstract_file_system {
    public:
        virtual bool exists(const std::u16string &path) = 0;
        virtual bool replace(const std::u16string &old_path, const std::u16string &new_path) = 0;

        /*! \brief Mount a drive with a physical host path.
        */
        virtual bool mount_volume_from_path(const drive_number drv, const drive_media media, const io_attrib attrib,
            const std::u16string &physical_path) {
            return false;
        }

        virtual bool unmount(const drive_number drv) = 0;

        virtual std::unique_ptr<file> open_file(const std::u16string &path, const int mode) = 0;
        virtual std::unique_ptr<directory> open_directory(const std::u16string &path, const io_attrib attrib) = 0;
        
        virtual std::optional<entry_info> get_entry_info(const std::u16string &path) = 0;

        virtual abstract_file_system_err_code is_entry_in_rom(const std::u16string &path) {
            return abstract_file_system_err_code::unsupported;
        }

        virtual bool delete_entry(const std::u16string &path) = 0;

        virtual bool create_directory(const std::u16string &path) = 0;
        virtual bool create_directories(const std::u16string &path) = 0;

        virtual std::optional<drive> get_drive_entry(const drive_number drv) = 0;

        virtual void set_product_code(const std::string &code) {
            return;
        }

        virtual void set_epoc_ver(const epocver ver) {
            return;
        }

        virtual std::optional<std::u16string> get_raw_path(const std::u16string &path) = 0;

        virtual std::int64_t watch_directory(const std::u16string &path, common::directory_watcher_callback callback,
            void *callback_userdata, const std::uint32_t filters) {
            return -1;
        }

        virtual bool unwatch_directory(const std::int64_t handle) {
            return false;
        }
    };

    std::shared_ptr<abstract_file_system> create_physical_filesystem(const epocver ver, const std::string &product_code);
    std::shared_ptr<abstract_file_system> create_rom_filesystem(loader::rom *rom_cache, memory_system *mem,
        const epocver ver, const std::string &product_code);

    using file_system_inst = std::shared_ptr<abstract_file_system>;
    using filesystem_id = std::size_t;

    class io_system {
        std::map<filesystem_id, file_system_inst> filesystems;
        std::mutex access_lock;

        std::atomic<filesystem_id> id_counter;

    public:
        void init();

        void set_product_code(const std::string &pc);
        void set_epoc_ver(const epocver ver);

        std::optional<std::u16string> get_raw_path(const std::u16string &path);

        /*! \brief Add a new file system to the IO system
        *
        * Each filesystem will be assigned an ID for management.
        * 
        * \returns The filesystem ID in the IO system if success.
        */
        std::optional<filesystem_id> add_filesystem(file_system_inst &inst);

        /*! \brief Remove the filesystem from the IO system
        */
        bool remove_filesystem(const filesystem_id id);

        /*! \brief Check if the file exist in VFS.
        *
        * Iterates through all filesystem. If at least one filesystem
        * report that this file exist, stop and returns true. Else
        * return false.
        */
        bool exist(const std::u16string &path);

        /*! \brief Replace the old file with new file.
        *
        * Iterates through all system and rename until at least one FS reports
        * that the operation success.
        * 
        * \params old_path The target path.
        * \params new_path The new path to replaced the old.
        */
        bool rename(const std::u16string &old_path, const std::u16string &new_path);

        /*! \brief Shutdown the IO system.
        */
        void shutdown();

        /*! \brief Mount a physical path.
        *
        * Call all filesystem trying to mount this drive. Continue
        * until all fail or one success.
        * 
        * \returns True if at least one file system can mount this drive.
        */
        bool mount_physical_path(const drive_number dvc, const drive_media media, const io_attrib attrib,
            const std::u16string &path);

        /*! \brief Unount a drive.
        *
        * Call all filesystem trying to unmount this drive. Continue
        * until all fail or one success.
        * 
        * \returns True if at least one file system can unmount this drive.
        */
        bool unmount(const drive_number dvc);

        /*! \brief Check if the entry provided is a directory
        *
        * \returns False if the entry doesn't exist or is not a directory.
        */
        bool is_directory(const std::u16string &path);

        /*! \brief Open the file in guest.
        *
        * \returns Null if the file doesn't exist or can't be open with given mode.
        */
        std::unique_ptr<file> open_file(std::u16string vir_path, int mode);

        /*! \brief Open the directory in guest.
        */
        std::unique_ptr<directory> open_dir(std::u16string vir_path,
            const io_attrib attrib = io_attrib::none);

        /*! \brief Get a drive info.
        */
        std::optional<drive> get_drive_entry(const drive_number drv);

        /*! \brief Get entry info of a directory/file/socket.
        *
        * Note that ROM file/directory is only given the raw attribute (that
        * Symbian use), if there is a ROM Filesystem added.
        * 
        * \returns Nullopt if the given path doesn't exist, else the info
        */
        std::optional<entry_info> get_entry_info(const std::u16string &path);

        /*! \brief Check the entry is in ROM or not
        *
        * All filesystems will returns an error code to either tell
        * the IO that it supported or is the entry actually in the filesystem,
        * here the case is ROM. 
        */
        bool is_entry_in_rom(const std::u16string &path);

        bool delete_entry(const std::u16string &path);

        bool create_directory(const std::u16string &path);

        bool create_directories(const std::u16string &path);

        std::int64_t watch_directory(const std::u16string &path, common::directory_watcher_callback callback,
            void *callback_userdata, const std::uint32_t filters);
        
        bool unwatch_directory(const std::int64_t handle);
    };

    symfile physical_file_proxy(const std::string &path, int mode);

    class ro_file_stream : public common::ro_stream {
        file *f_;

    public:
        explicit ro_file_stream(file *f)
            : f_(f) {
        }

        void seek(const std::int64_t amount, common::seek_where wh) override;
        bool valid() override;
        std::uint64_t left() override;
        uint64_t tell() const override;
        uint64_t size() override;

        std::uint64_t read(void *buf, const std::uint64_t read_size) override;
    };

    class wo_file_stream: public common::wo_stream {
        file *f_;

    public:
        explicit wo_file_stream(file *f)
            : f_(f) {
        }

        void seek(const std::int64_t amount, common::seek_where wh) override;
        bool valid() override;
        std::uint64_t left() override;
        uint64_t tell() const override;
        uint64_t size() override;

        void write(const void *buf, const std::uint32_t write_size) override;
    };
}
