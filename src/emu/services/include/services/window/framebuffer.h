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

#include <cstdint>
#include <cstring>
#include <vector>

namespace eka2l1::epoc {
    // A draw-device update can write the whole panel before a legacy client starts
    // writing narrower pixels through ScreenInfo. Do not count those earlier bytes
    // as evidence that the raw client writes at the wider depth.
    class raw_framebuffer_depth_observer {
        std::vector<std::uint8_t> baseline_;
        bool wide_ = false;

    public:
        void reset(const std::uint8_t *tail, std::size_t size) {
            baseline_.assign(tail, tail + size);
            wide_ = false;
        }

        void acknowledge_draw_device(const std::uint8_t *tail) {
            if (!baseline_.empty()) {
                std::memcpy(baseline_.data(), tail, baseline_.size());
            }
        }

        bool observe_raw(const std::uint8_t *tail) {
            wide_ = wide_ || (!baseline_.empty() && std::memcmp(baseline_.data(), tail, baseline_.size()) != 0);
            return wide_;
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
