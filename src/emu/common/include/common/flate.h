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
#include <vector>

namespace eka2l1 {
	/*! \brief Namespace contains all function related to Deflate/Inflate */
    namespace flate {

        // Made specificlly for Image Compressing
        enum {
            DEFLATE_LENGTH_MAG = 8,
            DEFLATE_DIST_MAG = 12,
            DEFLATE_MIN_LENGTH = 3,
            DEFLATE_MAX_LENGTH = DEFLATE_MIN_LENGTH - 1 + (1 << DEFLATE_LENGTH_MAG),
            DEFLATE_MAX_DIST = 1 << DEFLATE_DIST_MAG,
            DEFLATE_DIST_CODE_BASE = 0x200,
            DEFLATE_HASH_MUL = 0xACC4B9B19u,
            DEFLATE_HASH_SHIFT = 24,
            ENCODING_LITERALS = 256,
            ENCODING_LENGTHS = (DEFLATE_LENGTH_MAG - 1) * 4,
            ENCODING_SPECIALS = 1,
            ENCODING_DISTS = (DEFLATE_DIST_MAG - 1) * 4,
            ENCODING_LITERAL_LEN = ENCODING_LITERALS + ENCODING_LENGTHS + ENCODING_SPECIALS,
            ENCODING_EOS = ENCODING_LITERALS + ENCODING_LENGTHS,
            DEFLATE_CODES = ENCODING_LITERAL_LEN + ENCODING_DISTS

        };

        // Represent an encoding
        struct encoding {
            uint32_t lit_len[ENCODING_LITERAL_LEN];
            uint32_t dist[ENCODING_DISTS];
        };

		/*! \brief Represents a Deflate bit output */
        class bit_output {
            uint32_t code;
            uint32_t bits;
            uint8_t *start;
            uint8_t *end;

            void do_write(int bits, uint32_t size);

        public:
            bit_output();
            bit_output(uint8_t *buf, size_t size);

            virtual ~bit_output() {}
			
			/*! \brief Set the buffer to output. */
            void set(uint8_t *buf, size_t size);

			/*! \brief Get the pointer to the data. */
            uint8_t *data() const;
            uint32_t buffered_bits() const;

			/*! \brief Encode a value and write it to the Deflate stream. */
            void write(uint32_t val, uint32_t len);
            void huffman(uint32_t huff_code);
            void pad(uint32_t pad_size);
        };

		/*! \brief Represents a deflate bit input. */
        class bit_input {
            int count;
            uint32_t bits;
            int remain;
            const uint32_t *buf_ptr;

        public:
            bit_input();
            bit_input(const uint8_t *ptr, int len, int off = 0);
            virtual ~bit_input() {}
			
			/*! \brief Set the buffer to read from. */
            void set(const uint8_t *ptr, int len, int off = 0);

			/*! \brief Read a huffman byte. */
            uint32_t read();
			
            uint32_t read(size_t size);
            uint32_t huffman(const uint32_t *tree);
        };

        enum {
            HUFFMAN_MAX_CODELENGTH = 27,
            HUFFMAN_METACODE = HUFFMAN_MAX_CODELENGTH + 1,
            HUFFMAN_MAX_CODES = 0x8000
        };

		/*! \brief Contains Huffman decoding/encoding functions. */ 
        namespace huffman {
            bool huffman(const int *freq, uint32_t num_codes, int *huffman);
            void encoding(const int *huffman, uint32_t num_codes, int *encode_tab);
            void decoding(const int *huffman, uint32_t num_codes, uint32_t *decode_tree, int sym_base = 0);
            bool valid(const uint32_t *huffman, int num_codes);

            void externalize(bit_output &output, const int *huff_man, uint32_t num_codes);
            void internalize(bit_input &input, uint32_t *huffman, int num_codes);
        }

        enum {
            INFLATER_BUF_SIZE = 0x800,
            INFLATER_SAFE_ZONE = 8
        };

		/*! \brief Inflate a deflated stream (non-standard gzip). */
        class inflater {
            bit_input *bits;
            const uint8_t *rptr;
            int len;
            const uint8_t *avail;
            const uint8_t *limit;
            encoding encode;
            uint8_t out[DEFLATE_MAX_DIST]; 
            uint8_t huff[INFLATER_BUF_SIZE + INFLATER_SAFE_ZONE]; 

			/*! \brief Do inflation */
            int inflate();

        public:
		    /*! \brief Construct an inflate stream from a bit input */
            inflater(bit_input &input);
            ~inflater() {}

            void init();

			/*! \brief Inflate bytes of data. 
			 * 
			 * \param buf The destination to write inflated data to.
			 * \param rlen The number of byte to inflate 
			*/
            int read(uint8_t *buf, size_t rlen);
            
			/*! \brief Skip a number of bytes.
			 *
			 *  \param len Number of bytes to skip.
			*/
			int skip(int len);
        };
    }
}

