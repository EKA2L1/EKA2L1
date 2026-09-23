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

#include <cstdint>
#include <cstring>
#include <vector>

namespace eka2l1::epoc {
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
