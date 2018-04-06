#include <common/bytepair.h>
#include <cstdint>

namespace eka2l1 {
    namespace common {
        // Given a chunk of data compress by byte-pair compression as specified by Nokia,
        // decompress the chunk.
        // The game which I'm testing that use this type of compression (and even its pak file),
        // is Super Miners by ID
        int nokia_bytepair_decompress(void* dest, unsigned int dest_size, void* buffer, unsigned int buf_size) {
            uint8_t* data8 = reinterpret_cast<uint8_t*>(buffer);
            uint32_t lookup_table[0x50];

            uint8_t* lookup_table_u8 = reinterpret_cast<uint8_t*>(lookup_table);
            uint8_t* lookup_table_u8_1 = lookup_table_u8 + 0x100;

            // Fill the table
            uint32_t b = 0x03020100;
            uint32_t step = 0x04040404;

            for (uint32_t* lut = (uint32_t*)(lookup_table); b > step; lut++)
            {
                *lut = b;
                b += step;
            }

            uint8_t total_token = *data8++;

            if (total_token > 0) {
                uint8_t marker = *data8++;
                lookup_table_u8[marker] = ~marker;
            }

            // UNFINISHED

            return 1;
        }
    }
}
