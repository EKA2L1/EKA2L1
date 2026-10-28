/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <common/region.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace eka2l1::epoc {
    enum class framebuffer_update_source {
        draw_device,
        raw
    };

    // The tail is the part of a wide framebuffer beyond a complete narrow frame.
    // Attribute changes to the source submitting this update, then acknowledge
    // them for both sources. Each source retains its own depth until reset.
    class framebuffer_depth_tracker {
        std::vector<std::uint8_t> previous_tail_;
        bool draw_device_wide_ = false;
        bool raw_wide_ = false;

    public:
        void reset(const std::uint8_t *tail, std::size_t size) {
            if (size) {
                previous_tail_.assign(tail, tail + size);
            } else {
                previous_tail_.clear();
            }
            draw_device_wide_ = false;
            raw_wide_ = false;
        }

        bool update(framebuffer_update_source source, const std::uint8_t *tail) {
            bool &wide = source == framebuffer_update_source::draw_device ? draw_device_wide_ : raw_wide_;
            if (!previous_tail_.empty() && std::memcmp(previous_tail_.data(), tail, previous_tail_.size()) != 0) {
                wide = true;
                std::memcpy(previous_tail_.data(), tail, previous_tail_.size());
            }
            return wide;
        }
    };

    inline common::region explicit_framebuffer_update_region(const eka2l1::rect *symbian_rects,
        const std::uint32_t rect_count, const common::region &visible_region, const bool mapped_dsa) {
        // A mapped panel already contains the pixels being scanned out; draw-device
        // dirty rectangles do not describe writes made directly through that mapping.
        if (mapped_dsa) {
            return visible_region;
        }

        common::region dirty_region;
        for (std::uint32_t i = 0; i < rect_count; ++i) {
            eka2l1::rect area = symbian_rects[i];
            area.transform_from_symbian_rectangle();
            dirty_region.add_rect(area);
        }
        return dirty_region.intersect(visible_region);
    }

    // Observe a mapped panel without treating any pixel value as an unwritten marker.
    class framebuffer_observer {
        std::vector<std::uint8_t> previous_;
        std::vector<bool> written_rows_;
        std::size_t stride_ = 0;

    public:
        void reset(const std::uint8_t *data, std::size_t stride, std::size_t rows) {
            stride_ = stride;
            previous_.assign(data, data + stride * rows);
            written_rows_.assign(rows, false);
        }

        void acknowledge(const std::uint8_t *data) {
            std::memcpy(previous_.data(), data, previous_.size());
        }

        // A format change invalidates every row even when the bytes are unchanged.
        void invalidate() {
            std::fill(written_rows_.begin(), written_rows_.end(), true);
        }

        bool update(const std::uint8_t *data, std::size_t stride, std::size_t rows) {
            if (stride_ != stride || written_rows_.size() != rows) {
                reset(data, stride, rows);
                return false;
            }
            bool changed = false;
            for (std::size_t y = 0; y < rows; ++y) {
                auto *previous = previous_.data() + y * stride;
                const auto *current = data + y * stride;
                if (std::memcmp(previous, current, stride) != 0) {
                    std::memcpy(previous, current, stride);
                    written_rows_[y] = true;
                    changed = true;
                }
            }
            return changed;
        }

        const std::vector<bool> &written_rows() const { return written_rows_; }
    };
}
