/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <dispatch/libraries/buffer_pusher.h>

namespace eka2l1::dispatch {
    graphics_buffer_pusher::graphics_buffer_pusher() {
        current_buffer_ = 0;
        size_per_buffer_ = 0;
    }
    
    void graphics_buffer_pusher::add_buffer() {
        if (!size_per_buffer_) {
            return;
        }

        buffer_info info;
        info.data_ = new std::uint8_t[size_per_buffer_];
        info.used_size_ = 0;
        info.buffer_ = 0;

        buffers_.push_back(info);
    }

    void graphics_buffer_pusher::initialize(const std::size_t size_per_buffer) {
        if (!size_per_buffer) {
            return;
        }
        size_per_buffer_ = size_per_buffer;
        add_buffer();
    }

    void graphics_buffer_pusher::destroy(drivers::graphics_command_builder &builder) {
        for (std::size_t i = 0; i < buffers_.size(); i++) {
            if (buffers_[i].buffer_)
                builder.destroy(buffers_[i].buffer_);

            if (buffers_[i].data_) {
                delete buffers_[i].data_;
            }
        }
    }

    void graphics_buffer_pusher::flush(drivers::graphics_command_builder &builder) {
        for (std::uint8_t i = 0; i <= current_buffer_; i++) {
            if (i == buffers_.size()) {
                break;
            }

            const void *buffer_data_casted = buffers_[i].data_;
            const std::uint32_t buffer_size = static_cast<std::uint32_t>(buffers_[i].used_size_);

            if (buffer_size == 0) {
                continue;
            }

            builder.update_buffer_data_no_copy(buffers_[i].buffer_, 0, buffer_data_casted, buffer_size);
    
            buffers_[i].data_ = nullptr;
            buffers_[i].used_size_ = 0;
        }

        // Move on, these others might still be used
        if (size_per_buffer_ != 0) {
            current_buffer_++;
            
            if (current_buffer_ >= buffers_.size()) {
                add_buffer();
            }
        }
    }

    drivers::handle graphics_buffer_pusher::push_buffer(drivers::graphics_driver *drv, const std::uint8_t *data_source, const std::size_t total_buffer_size, std::size_t &buffer_offset) {
        if (buffers_[current_buffer_].used_size_ + total_buffer_size > size_per_buffer_) {
            current_buffer_++;
        }
        
        if (current_buffer_ == buffers_.size()) {
            add_buffer();
        }

        if (!buffers_[current_buffer_].buffer_) {
            buffers_[current_buffer_].buffer_ = drivers::create_buffer(drv, nullptr, size_per_buffer_, static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw));
        }

        if (!buffers_[current_buffer_].data_) {
            buffers_[current_buffer_].data_ = new std::uint8_t[size_per_buffer_];
        }

        buffer_offset = buffers_[current_buffer_].used_size_;
        std::memcpy(buffers_[current_buffer_].data_ + buffer_offset, data_source, total_buffer_size);

        buffers_[current_buffer_].used_size_ += (((total_buffer_size + 3) / 4) * 4);
        return buffers_[current_buffer_].buffer_;
    }

    void graphics_buffer_pusher::done_frame() {
        current_buffer_ = 0;
    }
}