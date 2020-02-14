/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <epoc/utils/err.h>
#include <epoc/services/fbs/compress_queue.h>
#include <epoc/services/fbs/fbs.h>

#include <common/log.h>
#include <common/runlen.h>

namespace eka2l1 {
    compress_queue::compress_queue(fbs_server *serv)
        : serv_(serv) {
        queue_.max_pending_count_ = 256;
    }

    void compress_queue::compress(fbsbitmap *bmp) {
        if (bmp->bitmap_->header_.compression != epoc::bitmap_file_no_compression) {
            // Why?
            bmp->compress_done_nof.complete(0);
            return;
        }

        queue_.push(bmp);
    }

    static epoc::bitmap_file_compression get_suitable_compression_method(fbsbitmap *bmp) {
        if (bmp->bitmap_->header_.palette_size != 0) {
            return epoc::bitmap_file_palette_compression;
        }

        switch (bmp->bitmap_->header_.bit_per_pixels) {
        case 8:
            return epoc::bitmap_file_byte_rle_compression;

        // For now, don't support 12
        //case 12:
        //    return epoc::bitmap_file_twelve_bit_rle_compression;
        
        case 16:
            return epoc::bitmap_file_sixteen_bit_rle_compression;

        case 24:
            return epoc::bitmap_file_twenty_four_bit_rle_compression;

        case 32:
            return epoc::bitmap_file_thirty_two_a_bit_rle_compression;
        }

        return epoc::bitmap_file_no_compression;
    }

    static std::size_t estimate_compress_size(fbsbitmap *bmp, std::uint8_t *data_base) {
        std::size_t est_size = 0;
        data_base += bmp->bitmap_->data_offset_;

        common::ro_buf_stream source(data_base, bmp->bitmap_->header_.bitmap_size - sizeof(loader::sbm_header));

        switch (bmp->bitmap_->header_.bit_per_pixels) {
        case 8:
            compress_rle<8>(&source, nullptr, est_size);
            break;

        case 16:
            compress_rle<16>(&source, nullptr, est_size);
            break;

        case 24:
            compress_rle<24>(&source, nullptr, est_size);
            break;

        case 32:
            compress_rle<32>(&source, nullptr, est_size);
            break;

        default:
            break;
        }

        return est_size;
    }

    static bool compress_data(fbsbitmap *bmp, std::uint8_t *base, std::uint8_t *dest_ptr, const std::size_t dest_size) {
        std::size_t est_size = 0;
        base += bmp->bitmap_->data_offset_;

        common::ro_buf_stream source(base, bmp->bitmap_->header_.bitmap_size - sizeof(loader::sbm_header));
        common::wo_buf_stream dest(dest_ptr, dest_size);

        bool result = false;

        switch (bmp->bitmap_->header_.bit_per_pixels) {
        case 8:
            result = compress_rle<8>(&source, &dest, est_size);
            break;

        case 16:
            result = compress_rle<16>(&source, &dest, est_size);
            break;

        case 24:
            result = compress_rle<24>(&source, &dest, est_size);
            break;

        case 32:
            result = compress_rle<32>(&source, nullptr, est_size);
            break;

        default:
            break;
        }

        return result;
    }

    void compress_queue::actual_compress(fbsbitmap *bmp) {
        fbsbitmap *clean_bitmap = bmp;
        epoc::bitmap_file_compression target_compression = get_suitable_compression_method(bmp);

        if (target_compression == epoc::bitmap_file_no_compression) {
            return;
        }

        if (bmp->support_dirty_bitmap) {
            // Have to create new bitmap
            clean_bitmap = serv_->create_bitmap(bmp->bitmap_->header_.size_pixels,
                bmp->bitmap_->settings_.current_display_mode(), false, true);
        }

        clean_bitmap->bitmap_->header_.compression = target_compression;
        clean_bitmap->bitmap_->compressed_in_ram_ = true;

        // Estimate compressed size
        std::uint8_t *data_base = serv_->get_large_chunk_base();
        const std::size_t estimated_size = estimate_compress_size(bmp, data_base);
        const std::size_t org_size = bmp->bitmap_->header_.bitmap_size - sizeof(loader::sbm_header);

        std::uint8_t *new_data = reinterpret_cast<std::uint8_t*>(serv_->allocate_large_data(estimated_size));
        const bool compress_result = compress_data(bmp, data_base, new_data, estimated_size);

        if (!compress_result) {
            LOG_ERROR("Unable to compress bitmap {}", bmp->id);

            // Cleanup
            serv_->free_large_data(new_data);
            
            if (bmp->support_dirty_bitmap) {
                serv_->free_bitmap(clean_bitmap);
            }

            return;
        }

        clean_bitmap->bitmap_->data_offset_ = static_cast<int>(new_data - data_base);
        clean_bitmap->bitmap_->header_.bitmap_size = static_cast<std::uint32_t>(estimated_size +
            sizeof(loader::sbm_header));

        // Notify dirty bitmaps
        {
            const std::lock_guard<std::mutex> guard(notify_mutex_);
            for (auto notify: notifies_) {
                notify.complete(0);
            }

            notifies_.clear();
        }

        // Notify bitmap compression done. Now the thread can run.
        bmp->compress_done_nof.complete(epoc::error_none);
        bmp->clean_bitmap = clean_bitmap;

        // Mark old bitmap as dirty
        bmp->bitmap_->settings_.dirty_bitmap(true);

        LOG_TRACE("Bitmap ID {} compressed with ratio {}%, clean bitmap ID {}", bmp->id, static_cast<int>(static_cast<double>(estimated_size) /
            static_cast<double>(org_size) * 100.0), clean_bitmap->id);
    }

    void compress_queue::run() {
        while (auto bmp = queue_.pop()) {
            actual_compress(bmp.value());
        }
    }

    void compress_queue::abort() {
        queue_.abort();
    }

    void compress_queue::notify(epoc::notify_info &nof) {
        const std::lock_guard<std::mutex> guard(notify_mutex_);
        notifies_.push_back(nof);
    }

    bool compress_queue::cancel(epoc::notify_info &nof) {
        const std::lock_guard<std::mutex> guard(notify_mutex_);
        auto find_res = std::find(notifies_.begin(), notifies_.end(), nof);

        if (find_res == notifies_.end()) {
            return false;
        }

        notifies_.erase(find_res);
        nof.complete(epoc::error_cancel);
        return true;
    }
}