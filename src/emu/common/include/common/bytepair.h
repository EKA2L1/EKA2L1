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
        class ro_stream;

        /*! \brief Given a chunk of data compress by byte-pair compression, decompress the chunk.
		 *
		 *  \param dest The destination to write decompressed data to
         *  \param dest_size The size of the destination buffer
         *  \param buffer The compressed data
         *  \param buf_size The compressed data size		 
		*/
        int bytepair_decompress(void *dest, unsigned int dest_size, void *buffer, unsigned int buf_size);

        enum {
            BYTEPAIR_PAGE_SIZE = 4096
        };

        /*! \brief A read-only bytepair stream. */
        class ibytepair_stream {
            common::ro_stream *compress_stream;

        public:
            struct index_table_header {
                int size_of_data;
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
            explicit ibytepair_stream(common::ro_stream *stream);

            /*! \brief Get the index table */
            index_table table() const;

            /** \brief Seek forward the stream */
            void seek_fwd(size_t size);

            /**
             * \brief Read the index table
			 *
			 * Bytepair compressed data always has a header (index table), that tells us the uncompressed size
			 * and each bytepair page's size.
			 */
            void read_table();

            /*! \brief Read a page.
			 *
			 *  \param page The page index
			 *  \param size The page size
			*/
            uint32_t read_page(char *dest, uint32_t page, size_t size);

            /*! \brief Read all available pages.
			 *
			 *  \param dest The destination to write decompressed data to 
			 *  \param size The destination size 
			*/
            uint32_t read_pages(char *dest, size_t size);

            /*! \brief Get all the pages's offsets */
            std::vector<uint32_t> page_offsets(uint32_t initial_off);
        };
    }
}
