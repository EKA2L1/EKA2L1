#pragma once

#include <kernel/chunk.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1 {
    namespace epoc {
        /*! \brief The reason that allocation fail. */
        enum class alloc_fail_reason {
            random,
            true_random,
            deterministic,
            none,
            fail_next,
            reset,
            burst_random,
            burst_true_random,
            burst_deterministic,
            burst_fail_next,
            check_fail
        };

        /*! \brief Symbian allocator. 
         * 
         * This is the higher base class for all Symbian allocator to based on, such as
         * RHeap or RHybridHeap. In Symbian, class provides virtual method to worked on.
         */
        struct allocator {
            //! Vtable
            eka2l1::ptr<int> vtable;

            //! Count of access time.
            /*! Each time RAllocator::Open or RAllocator::Close is called, this variable is atomicly
             * increase or decrease.
            */
            int access_count;

            //! Total handle of the handles array
            /*! This should always be one, since only the handle is contained.
            */
            int handle_count = 1;

            //! Contains handles that allocator implementation use
            /*! Although this in Symbian source supposed to contains multiple handles, 
                this actually only contains the chunk handle. 
            */
            eka2l1::ptr<int> handles;

            //! Allocator flags
            uint32_t flags;

            //! Total cell allocated
            /*! In Symbian, each allocation is stored and called cell. This is increase or decrease when
             * allocation or free is done.
            */
            int cell_count;

            //! Total size allocated.
            /*! This equals to all cell size allocated.
            */
            int total_alloc_size;
        };

        /*! \brief Symbian heap allocator.
         *
         */
        struct heap : public allocator {
            int unused1;
            int max_len;
            int unused2;
            int unused3;

            //! Unused (OSS Symbian)
            int chunk_handle;

            //! Unused (OSS Symbian)
            eka2l1::ptr<void> fast_lock;

            //! Heap base. The pointer to the begging of heap
            /*! This is also the base of heap chunk.
            */
            eka2l1::ptr<uint8_t> base;

            //! The top of the heap.
            /*! This is identical to the chunk top.
            */
            eka2l1::ptr<uint8_t> top;

            //! Allocation alignment
            int align;

            //! The reason allocation failed.
            alloc_fail_reason fail_reason;

            //! Unused
            int nesting_level;

            int test_data;
        };
    }
}

namespace eka2l1 {
    class memory_system;

    using chunk_ptr = std::shared_ptr<kernel::chunk>;

    /*! \brief A HLE heap block. Also known as cell in LLE.
     *
     * This is used for alllocation management in fast heap.
    */
    struct heap_block {
        //! Offset of the cell in the chunk
        uint32_t offset;

        //! Size of the cell
        /*! This size and the offset, chunk pointer can be changed
         * when doing reallocation.
        */
        uint32_t size;

        //! Pointer to the cell in the heap chunk.
        eka2l1::ptr<void> block_ptr;

        //! Chunk is free or not
        bool free = true;
    };

    /*! \brief Use for allocating from a chunk. 
     *
     * The very first bytes of the chunk contains the original RHeap. This
     * is for function from LLE client to get allocate information (total alloc size, etc...).
     *
     * Usually use for HLE services, where chunk and heap allocator are shared globally, assumed that
     * client don't touch any virtual allocate/free functions.
     */
    class fast_heap {
        //! The heap blocks
        /*! This contains all the cell. We can delete, insert more
         * cell in here.
        */
        std::vector<heap_block> heap_blocks;

        //! Pointer to the heap chunk.
        chunk_ptr chunk;

        //! Pointer to the memory system
        memory_system *mem;

        //! Pointer to the LLE RHeap in the chunk.
        epoc::heap *heap;

    public:
        fast_heap() = default;

        /*! \brief Construct the fast heap. */
        fast_heap(memory_system *mem_sys, chunk_ptr chunk, int dealign);

        /*! \brief Allocate from the heap. 
          * 
          * \param size The size of the cell to allocate.
          * \returns The pointer to the cell.
        */
        eka2l1::ptr<void> allocate(size_t size);

        /*! \brief Free the cell. 
         *
         * \param ptr The pointer to the cell.
         * \returns True if the cell can be found and free.
        */
        bool free(eka2l1::ptr<void> ptr);
    };
}