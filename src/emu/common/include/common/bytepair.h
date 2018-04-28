#pragma once

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>

namespace eka2l1 {
    namespace common {
        // Given a chunk of data compress by byte-pair compression as specified by Nokia,
        // decompress the chunk.
        int nokia_bytepair_decompress(void* dest, unsigned int dest_size, void* buffer, unsigned int buf_size);

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
                int	size_of_data;   // Includes the index and compressed pages
                int	decompressed_size;
                uint16_t	number_of_pages;
            };

            struct index_table {
                index_table_header header;
                std::vector<uint16_t> page_size;
            };

        private:

            index_table idx_tab;

        public:
            ibytepair_stream(std::shared_ptr<std::istream> stream);
            index_table table() const;

            void seek_fwd(size_t size);

            // Read the table entry
            void read_table();
            uint32_t read_page(char* dest, uint32_t page, size_t size);
            uint32_t read_pages(char* dest, size_t size);

            std::vector<uint32_t> page_offsets(uint32_t initial_off);
        };
    }
}
