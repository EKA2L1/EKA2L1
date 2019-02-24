#include <common/algorithm.h>
#include <common/allocator.h>

#include <algorithm>
#include <exception>
#include <stdexcept>

namespace eka2l1::common {
    block_allocator::block_allocator(std::uint8_t *sptr, const std::size_t initial_max_size)
        : space_based_allocator(sptr, initial_max_size) {
        const auto alignment_needed = 4 - reinterpret_cast<std::uint64_t>(ptr) % 4;

        if (alignment_needed > initial_max_size) {
            if (!expand(alignment_needed)) {
                throw std::runtime_error("Pointer is not align, expand to align failed!");
            }
        }

        ptr += alignment_needed;
    }

    void *block_allocator::allocate(std::size_t bytes) {
        std::size_t rounded_size = common::next_power_of_two(bytes);
        std::uint64_t farest_end_offset = 0;

        for (auto &block : blocks) {
            farest_end_offset = common::max(farest_end_offset, block.offset + block.size);

            if (!block.active && block.size >= rounded_size) {
                // Gonna use it right away
                if (block.size == rounded_size) {
                    block.active = true;
                    return block.offset + ptr;
                }

                void *ret_ptr = ptr + block.offset;

                // Divide it to two if we can
                block_info new_block;
                new_block.active = true;
                new_block.offset = block.offset;
                new_block.size = rounded_size;

                // Change our old block
                block.size -= rounded_size;
                block.offset += rounded_size;

                blocks.push_back(std::move(new_block));
                return ret_ptr;
            }
        }

        if (farest_end_offset >= max_size) {
            // It's time to expand
            if (!expand(max_size * 2)) {
                return nullptr;
            }
        }

        // We try our best, but we can't
        // We should alloc new block
        block_info new_block;
        new_block.active = true;
        new_block.offset = farest_end_offset;
        new_block.size = rounded_size;

        blocks.push_back(std::move(new_block));

        return farest_end_offset + ptr;
    }

    bool block_allocator::free(const void *tptr) {
        const std::uint64_t to_free_offset = reinterpret_cast<const std::uint8_t *>(tptr) - ptr;

        auto ite = std::find_if(blocks.begin(), blocks.end(),
            [to_free_offset](const block_info &info) {
                return info.active && info.offset == to_free_offset;
            });

        if (ite == blocks.end()) {
            return false;
        }

        ite->active = false;
        return true;
    }
}