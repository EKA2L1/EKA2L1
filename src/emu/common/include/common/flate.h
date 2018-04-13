#pragma once

#include <vector>
#include <cstdint>

namespace eka2l1 {
    namespace flate {
        class bit_output {
            uint32_t code;
            uint32_t bits;
            uint8_t* start;
            uint8_t* end;

            void do_write(int bits, uint32_t size);

        public:
            bit_output();
            bit_output(uint8_t* buf, size_t size);

            virtual ~bit_output() {}
            void set(uint8_t* buf, size_t size);

            uint8_t* data() const;
            uint32_t buffered_bits() const;

            void write(uint32_t val, uint32_t len);
            void huffman(uint32_t huff_code);
            void pad(uint32_t pad_size);
        };

        class bit_input {
            size_t   count;
            uint32_t bits;
            size_t   remain;
            const uint32_t buf_ptr;
        public:
            bit_input();
            bit_input(const uint8_t* ptr, int len, int off = 0);
            virtual ~bit_input() {}
            void set(const uint8_t* ptr, int len, int off = 0);

            uint32_t read();
            uint32_t read(size_t size);
            uint32_t huffman(const uint32_t* tree);
        };

        enum {
            HUFFMAN_MAX_CODELENGTH = 27,
            HUFFMAN_METACODE = HUFFMAN_MAX_CODELENGTH + 1,
            HUFFMAN_MAX_CODES = 0x8000
        };

        namespace huffman {
            bool huffman(const int* freq, uint32_t num_codes, int* huffman);
            void encoding(const int* huffman, uint32_t num_codes, int* encode_tab);
            void decoding(const int* huffman, uint32_t num_codes, uint32_t* decode_tree,int sym_base = 0);
            bool valid(const int* huffman,int num_codes);

            void externalize(bit_output& aOutput,const uint32_t* huff_man, uint32_t num_codes);
            void externalize(bit_input& aInput, uint32_t* huffman,int num_codes);
        }
    }
}
