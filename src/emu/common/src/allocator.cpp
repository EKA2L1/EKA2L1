#include <common/algorithm.h>
#include <common/allocator.h>

#include <algorithm>
#include <exception>
#include <stdexcept>

namespace eka2l1::common {
    block_allocator::block_allocator(std::uint8_t *sptr, const std::size_t initial_max_size)
        : space_based_allocator(sptr, initial_max_size) {
        const auto alignment_needed = (4 - reinterpret_cast<std::uint64_t>(ptr) % 4) % 4;

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
            if (!expand(common::max(max_size * 2, rounded_size))) {
                return nullptr;
            }

            max_size = common::max(max_size * 2, rounded_size);
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

    bitmap_allocator::bitmap_allocator(const std::size_t total_bits)
        : words_((total_bits >> 5) + ((total_bits % 32 != 0) ? 1 : 0), 0xFFFFFFFF) {
    }

    void bitmap_allocator::force_fill(const int offset, const int size, const bool or_mode) {
        std::uint32_t *word = &words_[0] + (offset >> 5);
        const int set_bit = offset & 31;
        int end_bit = set_bit + static_cast<int>(size);

        if (end_bit < 32) {
            // The bit we need to allocate is in single word
            const std::uint32_t mask = (~(0xFFFFFFFFU >> size) >> set_bit);
            *word = or_mode ? (*word | mask) : (*word & ~mask);

            return;
        }

        // All the bits trails across some other words
        std::uint32_t mask = 0xFFFFFFFFU >> set_bit;

        while (end_bit > 0) {
            const std::uint32_t w = *word;
            *word++ = or_mode ? (w | mask) : (w & ~mask);

            // We only need to be careful with the first word, since it only fills
            // some first bits. We should fully fill with other word, so set the mask full
            mask = 0xFFFFFFFFU;
            end_bit -= 32;

            if (end_bit < 32) {
                mask = ~(mask >> end_bit);
            }
        }
    }

    void bitmap_allocator::free(const int offset, const int size) {
        force_fill(offset, size, true);
    }

    int bitmap_allocator::allocate_from(const int start_offset, const int size, const bool best_fit) {
        std::uint32_t *word = &words_[0] + (start_offset << 5);

        // We have arrived at le word that still have free position (bit 1)
        std::uint32_t *word_end = &words_[words_.size() - 1];
        int soc = start_offset;

        int bflmin = 0xFFFFFF;
        int bofmin = -1;
        std::uint32_t *wordmin = nullptr;

        // Keep finding
        while (word <= word_end) {
            std::uint32_t wv = *word;

            if (wv != 0) {
                // Still have free stuff
                int bflen = 0;
                int boff = 0;
                std::uint32_t *bword = nullptr;

                // Start from the most significant bit 1
                int cursor = common::max(31 - soc & 31, common::find_most_significant_bit_one(wv) - 1);

                while (cursor > 0) {
                    if (((wv >> cursor) & 1) == 1) {
                        boff = cursor;
                        bflen = 0;
                        bword = word;
                        
                        while (cursor >= 0 && ((wv >> cursor) & 1) == 1) {
                            bflen++;
                            cursor--;

                            if (cursor < 0 && ++word <= word_end) {
                                cursor = 31;
                                soc = 32;
                                wv = *word;
                            }
                        }

                        if (bflen >= size) {
                            if (!best_fit) {
                                // Force allocate and then return
                                const int offset = static_cast<int>(31 - boff + ((bword - &words_[0]) << 5));
                                force_fill(offset, size, false);

                                return offset;
                            } else {
                                if (bflen < bflmin) {
                                    bflmin = bflen;
                                    bofmin = boff;
                                    wordmin = bword;
                                }
                            }
                        }
                    }
                
                    cursor--;
                }
            }

            soc = 32;
            word++;
        }

        if (best_fit && bofmin) {
            // Force allocate and then return
            const int offset = static_cast<int>(31 - bofmin + ((wordmin - &words_[0]) << 5));
            force_fill(offset, size, false);

            return offset;
        }
    
        return -1;
    }

    bool bitmap_allocator::set_word(const std::uint32_t off, const std::uint32_t val) {
        if (off >= words_.size()) {
            return false;
        }

        words_[off] = val;
        return true;
    }

    const std::uint32_t bitmap_allocator::get_word(const std::uint32_t off) const {
        if (off >= words_.size()) {
            return false;
        }

        return words_[off];
    }
}
