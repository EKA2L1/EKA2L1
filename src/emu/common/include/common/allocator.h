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

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace eka2l1::common {
    class allocator {
    public:
        virtual void *allocate(std::size_t s) = 0;
        virtual bool free(const void *ptr) = 0;
    };

    class space_based_allocator : public allocator {
    protected:
        std::uint8_t *ptr;
        std::size_t max_size;

    public:
        explicit space_based_allocator(std::uint8_t *sptr, const std::size_t initial_max_size)
            : ptr(sptr)
            , max_size(initial_max_size) {
        }

        /*! \brief Expand the space to new limit.
         *
         * \returns True if expandable.
        */
        virtual bool expand(std::size_t target_max_size) = 0;

        std::size_t get_max_size() const {
            return max_size;
        }
    };

    class block_allocator : public space_based_allocator {
        struct block_info {
            std::uint64_t offset;
            std::size_t size;

            bool active{ false };
        };

        std::vector<block_info> blocks;
        std::mutex lock;

    public:
        explicit block_allocator(std::uint8_t *sptr, const std::size_t initial_max_size);

        void *allocate(std::size_t bytes) override;
        bool free(const void *ptr) override;

        virtual bool expand(std::size_t target) override {
            return false;
        }
    };

    struct bitmap_allocator {
        std::vector<std::uint32_t> words_;

    public:
        // For testing, don't use this if not neccessary
        bool set_word(const std::uint32_t off, const std::uint32_t val);
        const std::uint32_t get_word(const std::uint32_t off) const;

        bitmap_allocator() = default;
        explicit bitmap_allocator(const std::size_t total_bits);

        void set_maximum(const std::size_t total_bits);

        int force_fill(const std::uint32_t offset, const int size, const bool or_mode = false);
        int allocate_from(const std::uint32_t start_offset, int &size, const bool best_fit = false);
        void free(const std::uint32_t offset, const int size);

        /**
         * @brief   Get the number of allocated cells from specified region.
         * 
         * @param   offset          Begin offset of the region.
         * @param   offset_end      End offset of the region.
         * 
         * @returns Number of cells already allocated on this region.
         */
        int allocated_count(const std::uint32_t offset, const std::uint32_t offset_end);
    };
}
