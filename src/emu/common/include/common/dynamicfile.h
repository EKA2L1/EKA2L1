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

#include <fstream>
#include <string>

namespace eka2l1::common {
    /**
     * \brief A wrapper around ifstream, support reading many encodings.
     *
     * This is a wrapper of fstream, supports reading UTF-8 and UCS-2 file
     * to either string or an 16-bit string
     * 
     * It was born to deal with system files in Symbian, where most of them
     * are saved as an UCS2 file.
     */
    class dynamic_ifile {
        std::ifstream stream_;
        int ucs2_;

    public:
        explicit dynamic_ifile(const std::string &name);

        /**
         * \brief Read a line to an 8-bit string
         *
         * String will be set to empty if the line is empty or file failed 
         */
        bool getline(std::string &line);

        /** 
         * \brief Read a line to u16string
         *
         * String will be set to empty if the line is empty or file failed 
         */
        bool getline(std::u16string &line);

        void read(std::string &line, const std::size_t len);

        void read(std::u16string &line, const std::size_t len);

        void seek(int mode, const std::size_t offset);

        bool eof() {
            return stream_.eof();
        }

        bool fail() {
            return stream_.fail();
        }

        inline bool is_ucs2() {
            return ucs2_ != -1;
        }
    };
}