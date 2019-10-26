#pragma once

#include <cstdint>

namespace eka2l1::epoc {
    /*! \brief Chunk create info.
     *
     * This struct is passed on creation of a chunk.
    */
    struct chunk_create {
        //! Attributes for chunk creation that are used by both euser and the kernel
        // by classes TChunkCreateInfo and SChunkCreateInfo, respectively.
        enum chunk_create_att {
            normal = 0x00000000,
            double_ended = 0x00000001,
            disconnected = 0x00000002,
            cache = 0x00000003,
            mapping_mask = 0x0000000f,
            local = 0x00000000,
            global = 0x00000010,
            data = 0x00000000,
            code = 0x00000020,
            memory_not_owned = 0x00000040,

            // Force local chunk to be named.  Only required for thread heap
            // chunks, all other local chunks should be nameless.
            local_named = 0x000000080,

            // Make global chunk read only to all processes but the controlling owner
            read_only = 0x000000100,

            // Paging attributes for chunks.
            paging_unspec = 0x00000000,
            paged = 0x80000000,
            unpaged = 0x40000000,
            paging_mask = paged | unpaged,

            chunk_create_att_mask = mapping_mask | global | code | local_named | read_only | paging_mask,
        };

    public:
        std::uint32_t att;
        std::uint8_t force_fixed;
        std::int32_t initial_bottom;
        std::int32_t initial_top;
        std::int32_t max_size;
        std::uint8_t clear_bytes;
    };
}