/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <services/fbs/palette.h>
#include <services/window/bitmap_cache.h>

#include <epoc/epoc.h>
#include <kernel/kernel.h>
#include <kernel/chunk.h>

#include <algorithm>

#include <common/buffer.h>
#include <common/log.h>
#include <common/runlen.h>
#include <common/time.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace eka2l1::epoc {
    bitmap_cache::bitmap_cache(kernel_system *kern_)
        : base_large_chunk(nullptr)
        , kern(kern_) {
        std::fill(driver_textures.begin(), driver_textures.end(), 0);
        std::fill(hashes.begin(), hashes.end(), 0);
    }

    bool is_palette_bitmap(epoc::bitwise_bitmap *bw_bmp) {
        const epoc::display_mode dsp = bw_bmp->settings_.current_display_mode();
        return (dsp == epoc::display_mode::color16) || (dsp == epoc::display_mode::color256);
    }

    static char *converted_bw_bitmap_to_twenty_four_bitmap(epoc::bitwise_bitmap *bw_bmp,
        const std::uint32_t *original_ptr, std::vector<char> &converted_pool) {
        std::uint32_t byte_width_converted = common::align(bw_bmp->header_.size_pixels.x * 3, 4);
        converted_pool.resize(byte_width_converted * bw_bmp->header_.size_pixels.y);

        char *return_ptr = &converted_pool[0];

        for (std::size_t y = 0; y < bw_bmp->header_.size_pixels.y; y++) {
            for (std::size_t x = 0; x < bw_bmp->header_.size_pixels.x; x++) {
                std::uint32_t word_per_line = (bw_bmp->header_.size_pixels.x + 31) / 32;
                std::uint32_t color = original_ptr[y * word_per_line + x / 32];
                std::uint32_t converted_color =  0;
                if (color & (1 << (x & 0x1F))) {
                    converted_color = 0xFFFFFF;
                }

                std::memcpy(return_ptr + byte_width_converted * y + x * 3, reinterpret_cast<const char *>(&converted_color), 3);
            }
        }
        return return_ptr;
    }

    static char *converted_palette_bitmap_to_twenty_four_bitmap(epoc::bitwise_bitmap *bw_bmp,
        const std::uint8_t *original_ptr, std::vector<char> &converted_pool) {
        std::uint32_t byte_width_converted = common::align(bw_bmp->header_.size_pixels.x * 3, 4);
        converted_pool.resize(byte_width_converted * bw_bmp->header_.size_pixels.y);

        char *return_ptr = &converted_pool[0];

        for (std::size_t y = 0; y < bw_bmp->header_.size_pixels.y; y++) {
            for (std::size_t x = 0; x < bw_bmp->header_.size_pixels.x; x++) {
                switch (bw_bmp->settings_.current_display_mode()) {
                case epoc::display_mode::color256: {
                    const std::uint8_t palette_index = original_ptr[y * bw_bmp->byte_width_ + x];
                    const std::uint32_t palette_color = epoc::color_256_palette[palette_index];

                    std::memcpy(return_ptr + byte_width_converted * y + x * 3, reinterpret_cast<const char *>(&palette_color), 3);

                    break;
                }

                default:
                    LOG_ERROR("Unhandled display mode to convert {}", static_cast<int>(bw_bmp->settings_.current_display_mode()));
                    break;
                }
            }
        }

        return return_ptr;
    }

    std::uint64_t bitmap_cache::hash_bitwise_bitmap(epoc::bitwise_bitmap *bw_bmp) {
        std::uint64_t hash = 0xB1711A3F;

        // Hash using XXHASH
        XXH64_state_t *const state = XXH64_createState();
        XXH64_reset(state, hash);

        // First, hash the single bitmap header
        XXH64_update(state, reinterpret_cast<const void *>(&bw_bmp->header_), sizeof(loader::sbm_header));

        // Now, hash the byte width and UID, if it changes, we need to update
        XXH64_update(state, reinterpret_cast<const void *>(&bw_bmp->byte_width_), sizeof(bw_bmp->byte_width_));
        XXH64_update(state, reinterpret_cast<const void *>(&bw_bmp->uid_), sizeof(bw_bmp->uid_));

        // Lastly, we needs to hash the data, to see if anything changed
        XXH64_update(state, base_large_chunk + bw_bmp->data_offset_, bw_bmp->header_.bitmap_size - sizeof(bw_bmp->header_));

        hash = XXH64_digest(state);
        XXH64_freeState(state);

        return hash;
    }

    std::int64_t bitmap_cache::get_suitable_bitmap_index() {
        // First time, will scans through the bitmap array to find empty box
        // Sometimes, app might purges a lot of bitmaps at same time
        for (std::int64_t i = MAX_CACHE_SIZE - 1; i >= 0; i--) {
            if (!bitmaps[i]) {
                return i;
            }
        }

        // Next, we will compare the timestamp
        std::uint64_t oldest_timestamp_idx = 0;
        for (std::int64_t i = 1; i < MAX_CACHE_SIZE; i++) {
            if (timestamps[i] < timestamps[oldest_timestamp_idx]) {
                oldest_timestamp_idx = i;
            }
        }

        return oldest_timestamp_idx;
    }

    drivers::handle bitmap_cache::add_or_get(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        epoc::bitwise_bitmap *bmp) {
        if (!base_large_chunk) {
            chunk_ptr ch = kern->get_by_name<kernel::chunk>("FbsLargeChunk");
            base_large_chunk = reinterpret_cast<std::uint8_t*>(ch->host_base());
        }

        std::int64_t idx = 0;
        std::uint64_t crr_timestamp = common::get_current_time_in_microseconds_since_1ad();

        std::uint64_t hash = 0;

        bool should_upload = true;

        auto bitmap_ite = std::find(bitmaps.begin(), bitmaps.end(), bmp);
        if (bitmap_ite == bitmaps.end()) {
            // If the bitmap is not in the bitmap array
            if (last_free < MAX_CACHE_SIZE) {
                // Use last free
                idx = last_free++;
            } else {
                idx = get_suitable_bitmap_index();
            }

            bitmaps[idx] = bmp;
            driver_textures[idx] = 0;
            hash = (hashes[idx] == 0) ? hash_bitwise_bitmap(bmp) : hashes[idx];
        } else {
            // Else, get the index
            idx = std::distance(bitmaps.begin(), bitmap_ite);

            // Check if we should upload or not, by calculating the hash
            hash = hash_bitwise_bitmap(bmp);
            should_upload = hash != (hashes[idx]);
        }

        if (should_upload) {
            if (driver_textures[idx])
                builder->destroy_bitmap(driver_textures[idx]);

            driver_textures[idx] = drivers::create_bitmap(driver, bmp->header_.size_pixels);
            char *data_pointer = reinterpret_cast<char *>(base_large_chunk + bmp->data_offset_);

            std::vector<std::uint8_t> decompressed;
            std::uint32_t raw_size = 0;

            if (bmp->header_.compression != bitmap_file_no_compression) {
                raw_size = bmp->byte_width_ * bmp->header_.size_pixels.y;
                decompressed.resize(raw_size);

                const std::uint32_t compressed_size = bmp->header_.bitmap_size - bmp->header_.header_len;

                common::wo_buf_stream dest_stream(&decompressed[0], raw_size);
                common::ro_buf_stream source_stream(reinterpret_cast<std::uint8_t *>(data_pointer), compressed_size);

                switch (bmp->header_.compression) {
                case bitmap_file_byte_rle_compression:
                    eka2l1::decompress_rle<8>(&source_stream, &dest_stream);
                    break;

                case bitmap_file_sixteen_bit_rle_compression:
                    eka2l1::decompress_rle<16>(&source_stream, &dest_stream);
                    break;

                case bitmap_file_twenty_four_bit_rle_compression:
                    eka2l1::decompress_rle<24>(&source_stream, &dest_stream);
                    break;

                default:
                    LOG_ERROR("Unsupported bitmap format to decode {}", bmp->header_.compression);
                    break;
                }

                data_pointer = reinterpret_cast<char *>(&decompressed[0]);
            } else {
                raw_size = bmp->header_.bitmap_size - bmp->header_.header_len;
            }

            std::vector<char> converted;
            std::uint32_t bpp = bmp->header_.bit_per_pixels;
            std::size_t pixels_per_line = 0;

            if ((bmp->header_.bit_per_pixels % 8) == 0) {
                pixels_per_line = bmp->byte_width_ / (bmp->header_.bit_per_pixels >> 3);
            }

            // GPU don't support them. Convert them on CPU
            if (is_palette_bitmap(bmp)) {
                data_pointer = converted_palette_bitmap_to_twenty_four_bitmap(bmp, reinterpret_cast<const std::uint8_t *>(data_pointer),
                    converted);
                bpp = 24;
                raw_size = static_cast<std::uint32_t>(converted.size());

                // Use default
                pixels_per_line = 0;
            }

            if (bpp == 1) {
                data_pointer = converted_bw_bitmap_to_twenty_four_bitmap(bmp, reinterpret_cast<const std::uint32_t *>(data_pointer),
                    converted);
                bpp = 24;
                raw_size = static_cast<std::uint32_t>(converted.size());

                // Use default
                pixels_per_line = 0;
            }

            builder->update_bitmap(driver_textures[idx], bpp, data_pointer, raw_size, { 0, 0 }, bmp->header_.size_pixels, pixels_per_line);
            hashes[idx] = hash;

            if (bmp->settings_.current_display_mode() == epoc::display_mode::color16mu) {
                builder->set_swizzle(driver_textures[idx], drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                    drivers::channel_swizzle::blue, drivers::channel_swizzle::one);
            }
        }

        timestamps[idx] = crr_timestamp;
        return driver_textures[idx];
    }
}
