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

#include <common/dynamicfile.h>
#include <common/cvt.h>

namespace eka2l1::common {
    dynamic_ifile::dynamic_ifile(const std::string &name)
        : stream_(name, std::ios::binary) {
        // Read the POM
        std::uint16_t pom = 0;

        stream_.read(reinterpret_cast<char*>(&pom), sizeof(pom));
        
        if (pom == 0xFEFF) {
            // Little-endian
            ucs2_ = 0;
        } else if (pom == 0xFFFE) {
            // Big-endian
            ucs2_ = 1;
        } else {
            ucs2_ = -1;
            stream_.seekg(0, std::ios::beg);
        }
    }
    
    bool dynamic_ifile::getline(std::string &line) {
        if (is_ucs2()) {
            // We need to get line from other method than do a convert
            std::u16string line_ucs2;
            bool res = getline(line_ucs2);

            line = common::ucs2_to_utf8(line_ucs2);

            return res;
        }

        // Read it until we get a newline
        return bool(std::getline(stream_, line));
    }
    
    bool dynamic_ifile::getline(std::u16string &line) {
        if (ucs2_ == -1) {
            std::string line_utf8;
            bool result(std::getline(stream_, line_utf8));

            line = common::utf8_to_ucs2(line_utf8);

            return result;
        }

        line.clear();
        
        while (true) {    
            char lo = 0;
            char hi = 0;
            
            if (ucs2_ = 0) {
                stream_.read(&hi, 1);
                stream_.read(&lo, 1);
            } else {
                stream_.read(&lo, 1);
                stream_.read(&hi, 1);
            }

            if (stream_.fail() || stream_.eof()) {
                return false;
            }

            if (static_cast<char16_t>((hi << 8) | lo) == u'\n') {
                break;
            }

            line.push_back((hi << 8) | lo);
        }

        return (!fail() && !eof());
    }

    void dynamic_ifile::read(std::string &line, const std::size_t len) {
        if (ucs2_ < 0) {
            stream_.read(&line[0], len);
            return;
        }

        // If it's an UCS2
        std::u16string d;
        read(d, len);

        line = common::ucs2_to_utf8(d);
    }

    void dynamic_ifile::read(std::u16string &line, const std::size_t len) {
        if (ucs2_ < 0) {
            std::string d;
            d.resize(len);

            stream_.read(&d[0], len);
            line = common::utf8_to_ucs2(d);
        }

        std::size_t iter = 0;

        while (iter < len) {    
            char lo = 0;
            char hi = 0;
            
            if (ucs2_ = 0) {
                stream_.read(&hi, 1);
                stream_.read(&lo, 1);
            } else {
                stream_.read(&lo, 1);
                stream_.read(&hi, 1);
            }

            if (static_cast<char16_t>((hi << 8) | lo) == u'\n') {
                break;
            }
            
            if (stream_.fail() || stream_.eof()) {
                return;
            }

            line.push_back((hi << 8) | lo);
        }
    }

    void dynamic_ifile::seek(int mode, const std::size_t offset) {
        std::ios::seekdir dir = std::ios::beg;

        if (mode == 1) {
            dir = std::ios::cur;
        }

        if (mode == 2) {
            dir = std::ios::end;
        }

        if (ucs2_ >= 0) {
            // Append 2 to skip POM
            // offset * 2 because it's an ucs2 file (4 bytes / character)
            stream_.seekg(2 + offset * 2, dir);
            return;
        }

        stream_.seekg(offset, dir);
        return;
    }
}