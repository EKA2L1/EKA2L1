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

#include <services/fbs/compress_queue.h>
#include <services/fbs/fbs.h>
#include <utils/err.h>

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
            bmp->compress_done_nof.complete(epoc::error_none);
            return;
        }

        if (bmp->support_dirty_bitmap) {
            // Have to create new bitmap
            fbs_bitmap_data_info info;
            info.dpm_ = bmp->bitmap_->settings_.current_display_mode();
            info.size_ = bmp->bitmap_->header_.size_pixels;

            clean_bitmap = serv_->create_bitmap(info, false, true);
        }

        // Estimate compressed size
        std::uint8_t *data_base = bmp->bitmap_->data_pointer(serv_);
        const std::size_t estimated_size = estimate_compress_size(bmp, data_base);
        const std::size_t org_size = bmp->bitmap_->header_.bitmap_size - sizeof(loader::sbm_header);

        if (estimated_size >= org_size) {
            // Stop compress
            if (bmp->support_dirty_bitmap) {
                serv_->free_bitmap(clean_bitmap);
            }

            bmp->compress_done_nof.complete(epoc::error_none);
            return;
        }

        std::uint8_t *new_data = nullptr;
        const bool is_large = serv_->is_large_bitmap(static_cast<std::uint32_t>(estimated_size));

        if (is_large) {
            new_data = reinterpret_cast<std::uint8_t *>(serv_->allocate_large_data(estimated_size));
        } else {    
            new_data = reinterpret_cast<std::uint8_t *>(serv_->allocate_general_data_impl(estimated_size));
        }

        const bool compress_result = compress_data(bmp, data_base, new_data, estimated_size);

        if (!compress_result) {
            LOG_ERROR(SERVICE_FBS, "Unable to compress bitmap {}", bmp->id);

            // Cleanup
            if (is_large)
                serv_->free_large_data(new_data);
            else
                serv_->free_general_data_impl(new_data);

            if (bmp->support_dirty_bitmap) {
                serv_->free_bitmap(clean_bitmap);
            }

            return;
        }

        if (serv_->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            std::uint8_t *org_pointer = clean_bitmap->original_pointer(serv_);

            if (clean_bitmap->bitmap_->offset_from_me_) {
                serv_->free_general_data_impl(org_pointer);
            } else {
                serv_->free_large_data(org_pointer);
            }
        }

        // Touch up the final clean
        clean_bitmap->bitmap_->header_.compression = target_compression;
        clean_bitmap->bitmap_->compressed_in_ram_ = true;

        if (is_large) {
            clean_bitmap->bitmap_->data_offset_ = static_cast<int>(new_data - serv_->get_large_chunk_base());
        } else {
            clean_bitmap->bitmap_->data_offset_ = static_cast<int>(new_data - reinterpret_cast<std::uint8_t*>(clean_bitmap->bitmap_));
        }

        if (serv_->legacy_level() == FBS_LEGACY_LEVEL_EARLY_EKA2) {
            clean_bitmap->bitmap_->settings_.set_large(is_large);
        }

        clean_bitmap->bitmap_->header_.bitmap_size = static_cast<std::uint32_t>(estimated_size + sizeof(loader::sbm_header));

        // Mark old bitmap as dirty
        if (bmp->support_dirty_bitmap) {
            clean_bitmap->ref();
            clean_bitmap->ref_extra_ed = true;

            bmp->clean_bitmap = clean_bitmap;
            bmp->bitmap_->settings_.dirty_bitmap(true);
        }
        
        // Notify bitmap compression done. Now the thread can run.
        bmp->compress_done_nof.complete(epoc::error_none);

        // Notify dirty bitmaps
        {
            const std::lock_guard<std::mutex> guard(notify_mutex_);
            for (auto notify : notifies_) {
                notify.complete(0);
            }

            notifies_.clear();
        }

        LOG_TRACE(SERVICE_FBS, "Bitmap ID {} compressed with ratio {}%, clean bitmap ID {}", bmp->id, static_cast<int>(static_cast<double>(estimated_size) / static_cast<double>(org_size) * 100.0), clean_bitmap->id);
    }

    void compress_queue::run() {
        kernel_system *kern = serv_->get_kernel_object_owner();

        while (auto bmp = queue_.pop()) {
            // Locking the kernel, we are doing quite some jobs
            kernel_lock guard(kern);
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