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

#include <cstdint>
#include <string>

namespace eka2l1::common {
    enum file_type {
        FILE_INVALID,
        FILE_UNKN,
        FILE_DEVICE,
        FILE_REGULAR,
        FILE_DIRECTORY
    };

    std::int64_t file_size(const std::string &path);
    file_type get_file_type(const std::string &path);

    bool is_file(const std::string &path, const file_type expected,
        file_type *result = nullptr);

    /* !\brief Resize given file.
     * \returns 0 if success
     *          -1 if file not found
     *          -2 if file resize failed (file is being readed, etc...)
    */
    int resize(const std::string &path, const std::uint64_t size);

    /* !\brief Remove a file.
    */
    bool remove(const std::string &path);

    bool move_file(const std::string &path, const std::string &new_path);

    struct dir_entry {
        file_type type;
        std::size_t size;

        std::string full_path;
    };

    /* !\brief An custom directory iterator that provides more detail if neccessary.
     *
     * Born to be portable later
    */
    struct dir_iterator {
        void *handle;
        void *find_data;

        bool eof;

        void cycles_to_next_entry();

    public:
        bool detail;

        explicit dir_iterator(const std::string &name);
        ~dir_iterator();

        bool is_valid() const;

        /*  \brief Get the next entry of the folder
         *  \returns 0 if success
         *          -1 if EOF
        */
        int next_entry(dir_entry &entry);
    };
}