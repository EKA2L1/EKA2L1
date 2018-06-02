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

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace common {
        // Given a chunk of data compress by byte-pair compression as specified by Nokia,
        // decompress the chunk.
        int nokia_bytepair_decompress(void *dest, unsigned int dest_size, void *buffer, unsigned int buf_size);

        enum {
            BYTEPAIR_PAGE_SIZE = 4096
        };

        // Nokia used a old compression algorithm because they are eager to try it out
        // when it was described with source code in 1994. This code is based both on
        // that source and Nokia's Open source Symbian Kernel
        class ibytepair_stream {
            std::shared_ptr<std::istream> compress_stream;

        public:
            struct index_table_header {
                int size_of_data; // Includes the index and compressed pages
                int decompressed_size;
                uint16_t number_of_pages;
            };

            struct index_table {
                index_table_header header;
                std::vector<uint16_t> page_size;
            };

        private:
            index_table idx_tab;

        public:
            ibytepair_stream(std::shared_ptr<std::istream> stream);
            ibytepair_stream(std::string path, uint32_t start);

            index_table table() const;

            void seek_fwd(size_t size);

            // Read the table entry
            void read_table();
            uint32_t read_page(char *dest, uint32_t page, size_t size);
            uint32_t read_pages(char *dest, size_t size);

            std::vector<uint32_t> page_offsets(uint32_t initial_off);
        };
    }
}

