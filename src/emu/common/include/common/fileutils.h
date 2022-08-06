/*
 * Copyright (c) 2018- EKA2L1 Team.
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

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace eka2l1::common {
    enum file_type {
        FILE_INVALID,
        FILE_UNKN,
        FILE_DEVICE,
        FILE_REGULAR,
        FILE_DIRECTORY
    };

    enum file_open_mode {
        FILE_OPEN_MODE_READ = 1 << 0,
        FILE_OPEN_MODE_WRITE = 1 << 1,
        FILE_OPEN_MODE_APPEND = 1 << 2
    };

    enum folder_copy_flags {
        FOLDER_COPY_FLAG_LOWERCASE_NAME = 1 << 0,
        FOLDER_COPY_FLAG_FILE_NO_OVERWRITE_IF_EXIST = 1 << 1
    };

    std::int64_t file_size(const std::string &path);
    file_type get_file_type(const std::string &path);

    std::uint64_t get_last_modifiy_since_ad(const std::u16string &path);

    bool is_file(const std::string &path, const file_type expected,
        file_type *result = nullptr);

    /**
     * \brief Resize given file.
     * \returns 0 if success
     *          -1 if file not found
     *          -2 if file resize failed (file is being readed, etc...)
     */
    int resize(const std::string &path, const std::uint64_t size);

    /* !\brief Remove a file.
    */
    bool remove(const std::string &path);

    bool move_file(const std::string &path, const std::string &new_path);

    /**
     * \brief Copy a file to target destination.
     * 
     * \param target_file               The file to be copied.
     * \param dest                      The destination.
     * \param overwrite_if_dest_exists  If this is true and the destination already exists, than the destination will be
     *                                  overwritten. Else, this function returns false.
     * 
     * \returns True on success.
     */
    bool copy_file(const std::string &target_file, const std::string &dest, const bool overwrite_if_dest_exists);

    /**
     * @brief   Copy target folder contents to another specified folder.
     * 
     * You can also use this function for folder transformation, by specifying the same path for target and dest.
     * 
     * @param   target_folder             Folder that needs to be copied. If this folder does not exist, false is returned.
     * @param   dest_folder_to_reside     Folder that will contain the copied folder. Automatically created if not exist.
     * @param   flags                     Extra flag specifying extra operations when doing the copy.
     * @param   progress                  Optional variable tracking progress of the copy.
     * 
     * @returns True on success.
     */
    bool copy_folder(const std::string &target_folder, const std::string &dest_folder_to_reside, const std::uint32_t flags, progress_changed_callback progress_callback = nullptr,
                     cancel_requested_callback cancel_cb = nullptr);

    bool delete_folder(const std::string &target_folder);

    bool is_system_case_insensitive();

    /*! \brief Create a directory. */
    void create_directory(std::string path);

    /*! \brief Check if a file or directory exists. */
    bool exists(std::string path);

    /*! \brief Check if the path points to a directory. */
    bool is_dir(std::string path);

    /*! \brief Create directories. */
    void create_directories(std::string path);

    bool set_current_directory(const std::string &path);
    bool get_current_directory(std::string &path);

    /**
     * @brief Open a C API file, handling through platform-specific API.
     * 
     * @param target_file       The path to the file for opening, in UTF-8.
     * @param mode              Open mode in string. This is the same as the one to be passed in fopen.
     * 
     * @returns Handle to the C file on success, else null.
     */
    FILE *open_c_file(const std::string &target_file, const char *mode);

    struct dir_entry {
        file_type type;
        std::size_t size;

        std::string name;
    };

    /**
     * \brief An custom directory iterator that provides more detail if neccessary.
     *
     * Born to be portable later
     */
    struct dir_iterator {
    public:
        std::string dir_name;
        bool detail;

        explicit dir_iterator(const std::string &dir)
            : dir_name(dir) {
        }

        virtual ~dir_iterator() = default;
        virtual bool is_valid() const = 0;

        /**
         * \brief Get the next entry of the folder
         * \returns 0 if success
         *          -1 if EOF
         */
        virtual int next_entry(dir_entry &entry) = 0;
    };

    /**
     * @brief Create a directory iterator that is suitable for the path format and the platform
     *        the program is running.
     *
     * Note that you still have to check if the iterator is valid even if the instance is not null,
     * by using the is_valid method.
     *
     * @param path The path to the folder we want to open the directory iterator.
     * @return Instance of the suitable directory iterator on success.
     */
    std::unique_ptr<dir_iterator> make_directory_iterator(const std::string &path);

    /**
     * @brief Given a filename, find the file/folder that matches this name by case-insensitive comparasion.
     * 
     * @param folder_path       The path of the folder to search for the insensitive filename/folder name.
     * @param insensitive_name  The insensitve file name to search
     * @param type              The type of entry to match (file or folder?)
     * 
     * @return std::string      Sensitive filename/folder name. Empty on not found.
     */
    std::string find_case_sensitive_file_name(const std::string &folder_path, const std::string &insensitive_name,
        const file_type type);
}
