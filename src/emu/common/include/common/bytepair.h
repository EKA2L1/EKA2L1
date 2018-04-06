#pragma once

namespace eka2l1 {
    namespace common {
        // Given a chunk of data compress by byte-pair compression as specified by Nokia,
        // decompress the chunk.
        int nokia_bytepair_decompress(void* dest, unsigned int dest_size, void* buffer, unsigned int buf_size);
    }
}
