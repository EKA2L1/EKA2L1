/*
 * Copyright (c) 2024 EKA2L1 Team.
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

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include <cassert>
#include <chrono>
#include <new>
#include <thread>

namespace eka2l1::drivers {

/**
 * \brief A simple bump-pointer arena allocator.
 *
 * Allocations are served sequentially from a fixed-size buffer. Individual
 * deallocations are not supported — the entire arena is reset at once.
 * This is ideal for per-frame command-list data that lives exactly from
 * CPU submission to GPU completion.
 */
class arena {
public:
    static constexpr std::size_t DEFAULT_SIZE = 8 * 1024 * 1024; // 8 MB

private:
    std::uint8_t *buffer_;
    std::size_t capacity_;
    std::size_t offset_;

public:
    arena()
        : buffer_(nullptr)
        , capacity_(0)
        , offset_(0) {
    }

    ~arena() {
        delete[] buffer_;
    }

    // Move-only
    arena(arena &&other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_)
        , offset_(other.offset_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.offset_ = 0;
    }

    arena &operator=(arena &&other) noexcept {
        if (this != &other) {
            delete[] buffer_;
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            offset_ = other.offset_;
            other.buffer_ = nullptr;
            other.capacity_ = 0;
            other.offset_ = 0;
        }
        return *this;
    }

    arena(const arena &) = delete;
    arena &operator=(const arena &) = delete;

    /**
     * \brief Initialise the arena with a fixed capacity.
     *
     * Must be called before any allocation. Can be called again to
     * resize (old data is lost).
     */
    void init(const std::size_t size) {
        delete[] buffer_;
        buffer_ = new std::uint8_t[size];
        capacity_ = size;
        offset_ = 0;
    }

    /**
     * \brief Allocate \p size bytes with \p alignment.
     *
     * \returns Pointer to the allocated memory, or nullptr on overflow.
     *   The caller should check for nullptr and fall back to heap if desired.
     */
    void *allocate(const std::size_t size, const std::size_t alignment = 8) {
        const std::size_t aligned_offset = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned_offset + size > capacity_) {
            return nullptr; // overflow — caller should fall back to heap
        }
        void *ptr = buffer_ + aligned_offset;
        offset_ = aligned_offset + size;
        return ptr;
    }

    /**
     * \brief Allocate an array of T with proper alignment.
     */
    template <typename T>
    T *allocate_array(const std::size_t count) {
        return static_cast<T *>(allocate(count * sizeof(T), alignof(T)));
    }

    /**
     * \brief Allocate and copy data from a source buffer.
     *
     * \returns Pointer to the copy inside the arena, or nullptr on overflow.
     */
    void *allocate_and_copy(const void *source, const std::size_t size, const std::size_t alignment = 8) {
        void *dest = allocate(size, alignment);
        if (dest) {
            std::memcpy(dest, source, size);
        }
        return dest;
    }

    /**
     * \brief Reset the arena so all space is available again.
     */
    void reset() {
        offset_ = 0;
    }

    /**
     * \brief Release ownership of the buffer (for pool destruction).
     *
     * After this call the arena is empty and the buffer is leaked —
     * used when the pool must be destroyed but the render thread may
     * still hold references to arena memory.
     */
    void release_ownership() {
        buffer_ = nullptr;
        capacity_ = 0;
        offset_ = 0;
    }

    /**
     * \brief Check whether a pointer falls within this arena's buffer.
     */
    bool owns(const void *ptr) const {
        return (ptr >= buffer_) && (ptr < buffer_ + capacity_);
    }

    std::size_t used() const { return offset_; }
    std::size_t remaining() const { return capacity_ - offset_; }
    std::size_t capacity() const { return capacity_; }
};

/**
 * \brief N-buffered arena pool.
 *
 * Manages a fixed pool of N arenas, allowing multiple concurrent builders
 * within a single thread to each acquire their own arena. Arenas are
 * released individually by the render thread after consuming the
 * associated command list.
 *
 * \tparam N  Pool size. Must be >= 2. Larger values allow more concurrent
 *            in-flight command lists before the CPU must wait for the GPU.
 *
 * Usage:
 *   cpu_thread:                          render_thread:
 *     arena *a = pool.acquire();           // ... after dispatching list ...
 *     // ... fill a ...                    pool.release(list.arena_);
 *     // a travels with command_list
 */
template <std::size_t N = 4>
class arena_pool {
    static_assert(N >= 2, "At least 2 arenas are required");

private:
    arena arenas_[N];

    // Bitmask tracking which arenas are currently in flight.
    // Bit i set => arenas_[i] is in use (CPU writing or GPU reading).
    std::atomic<std::uint32_t> in_flight_mask_{0};

    // Hint for which arena to try next (avoids always scanning from 0).
    std::atomic<int> next_hint_{0};

    std::size_t arena_size_;

public:
    arena_pool(const std::size_t arena_size = arena::DEFAULT_SIZE)
        : arena_size_(arena_size) {
        for (std::size_t i = 0; i < N; ++i) {
            arenas_[i].init(arena_size);
        }
    }

    ~arena_pool() {
        // Block until all arenas are returned by the render thread.
        // This is safe because: (a) in normal operation the render thread
        // processes lists in microseconds; (b) during orderly shutdown the
        // render thread is drained before any pool-owning object is destroyed.
        while (in_flight_mask_.load(std::memory_order_acquire) != 0) {
            std::this_thread::yield();
        }
    }

    /**
     * \brief Acquire a free arena (CPU thread).
     *
     * Tries a few times; if all arenas remain busy, returns nullptr.
     * The caller should fall back to heap allocation — this avoids
     * deadlocks when multiple threads hold arenas and wait on each other.
     *
     * \returns An arena pointer, or nullptr if the pool is exhausted.
     */
    arena *acquire() {
        for (int attempt = 0; attempt < 4; ++attempt) {
            std::uint32_t mask = in_flight_mask_.load(std::memory_order_acquire);

            // Find a free arena, starting from the hint.
            for (std::size_t offset = 0; offset < N; ++offset) {
                int idx = static_cast<int>((next_hint_.load(std::memory_order_relaxed) + offset) % N);
                std::uint32_t bit = 1u << idx;

                if (!(mask & bit)) {
                    // Try to claim it atomically.
                    if (in_flight_mask_.compare_exchange_weak(mask, mask | bit,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        next_hint_.store(static_cast<int>((idx + 1) % N), std::memory_order_relaxed);
                        arenas_[idx].reset();
                        return &arenas_[idx];
                    }
                    // CAS failed — another thread claimed it. Retry.
                    break;
                }
            }

            // All busy — brief yield then retry. After exhausting attempts,
            // return nullptr so the caller can fall back to heap mode.
            std::this_thread::yield();
        }

        return nullptr;
    }

    /**
     * \brief Release an arena back to the pool (render thread).
     *
     * Must be called after the GPU has finished reading from this arena.
     */
    void release(arena *a) {
        for (std::size_t i = 0; i < N; ++i) {
            if (&arenas_[i] == a) {
                in_flight_mask_.fetch_and(~(1u << i), std::memory_order_release);
                return;
            }
        }
    }

    std::size_t arena_size() const { return arena_size_; }
};

/**
 * \brief Type-erased arena release callback for arena_pool<N>.
 *
 * Use with command_list::set_arena(arena*, void*, arena_release_func)
 * to let the render thread return an arena to a specific private pool
 * (e.g. owned by an egl_context or canvas_base).
 */
template <std::size_t N>
inline void arena_pool_release(void *pool_tag, arena *a) {
    static_cast<arena_pool<N> *>(pool_tag)->release(a);
}

/**
 * \brief Helper: release heap-allocated data only if it is NOT owned by the arena.
 *
 * In arena mode, data pointers inside commands point into the arena buffer
 * and must not be individually freed — the entire arena is recycled at once.
 *
 * In heap mode (arena == nullptr), data is freed as before.
 *
 * \param a     The arena that may own the data (nullptr = heap mode).
 * \param ptr   The pointer to conditionally free.
 */
inline void release_data_if_not_arena(arena *a, void *ptr) {
    if (!ptr) return;
    if (!a || !a->owns(ptr)) {
        delete[] static_cast<std::uint8_t *>(ptr);
    }
}

} // namespace eka2l1::drivers
