/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <common/algorithm.h>
#include <common/allocator.h>
#include <common/log.h>

#include <algorithm>
#include <exception>
#include <iostream>
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

        const std::lock_guard<std::mutex> guard(lock);

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

        if (farest_end_offset + bytes > max_size) {
            // It's time to expand
            if (!expand(common::max(max_size * 2, max_size + rounded_size))) {
                return nullptr;
            }

            max_size = common::max(max_size * 2, max_size + rounded_size);
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

        const std::lock_guard<std::mutex> guard(lock);

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

    void bitmap_allocator::set_maximum(const std::size_t total_bits) {
        const std::size_t total_before = words_.size();
        const std::size_t total_after = (total_bits >> 5) + ((total_bits % 32 != 0) ? 1 : 0);

        words_.resize(total_after);

        if (total_after > total_before) {
            for (std::size_t i = total_before; i < total_after; i++) {
                words_[i] = 0xFFFFFFFFU;
            }
        }
    }
    
    int bitmap_allocator::force_fill(const std::uint32_t offset, const int size, const bool or_mode) {
        std::uint32_t *word = &words_[0] + (offset >> 5);
        const std::uint32_t set_bit = offset & 31;
        int end_bit = static_cast<int>(set_bit + size);

        std::uint32_t wval = *word;

        if (end_bit < 32) {
            // The bit we need to allocate is in single word
            const std::uint32_t mask = ((~(0xFFFFFFFFU >> size)) >> set_bit);

            if (or_mode) {
                *word = wval | mask;
            } else {
                *word = wval & (~mask);
            }

            return std::min<int>(size, static_cast<int>((words_.size() << 5) - set_bit));
        }

        // All the bits trails across some other words
        std::uint32_t mask = 0xFFFFFFFFU >> set_bit;

        while (end_bit > 0 && word != words_.data() + words_.size()) {
            wval = *word;

            if (or_mode) {
                *word = wval | mask;
            } else {
                *word = wval & (~mask);
            }
            
            word += 1; 

            // We only need to be careful with the first word, since it only fills
            // some first bits. We should fully fill with other word, so set the mask full
            mask = 0xFFFFFFFFU;
            end_bit -= 32;

            if (end_bit < 32) {
                mask = ~(mask >> static_cast<std::uint32_t>(end_bit));
            }
        }

        return std::min<int>(size, static_cast<int>((words_.size() << 5) - set_bit));
    }

    void bitmap_allocator::free(const std::uint32_t offset, const int size) {
        force_fill(offset, size, true);
    }

// Release code generation is corrupted somewhere on MSVC. Force fill is good so i guess it's the other.
// Either way, until when i can repro this in a short code, files and bug got fixed, this stays here.
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
    int bitmap_allocator::allocate_from(const std::uint32_t start_offset, int &size, const bool best_fit) {
        std::uint32_t *word = &words_[0] + (start_offset << 5);

        // We have arrived at le word that still have free position (bit 1)
        std::uint32_t *word_end = &words_[words_.size() - 1];
        std::uint32_t soc = start_offset;

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

                int cursor = 31;

                while (cursor > 0) {
                    if (((wv >> cursor) & 1) == 1) {
                        boff = cursor;
                        bflen = 0;
                        bword = word;
                        
                        while (cursor >= 0 && ((wv >> cursor) & 1) == 1) {
                            bflen++;
                            cursor--;

                            if (cursor < 0 && word + 1 <= word_end) {
                                cursor = 31;
                                soc = 32;
                                word++;
                                wv = *word;
                            }
                        }

                        if (bflen >= size) {
                            if (!best_fit) {
                                // Force allocate and then return
                                const int offset = static_cast<int>(31 - boff + ((bword - &words_[0]) << 5));
                                size = force_fill(static_cast<const std::uint32_t>(offset), size, false);

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
            size = force_fill(static_cast<std::uint32_t>(offset), size, false);

            return offset;
        }
    
        return -1;
    }
    
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

    bool bitmap_allocator::set_word(const std::uint32_t off, const std::uint32_t val) {
        if (off >= words_.size()) {
            return false;
        }

        words_[off] = val;
        return true;
    }

    const std::uint32_t bitmap_allocator::get_word(const std::uint32_t off) const {
        if (off >= words_.size()) {
            return 0;
        }

        return words_[off];
    }
}
