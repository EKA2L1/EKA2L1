/*
 * Copyright (c) 2018 EKA2L1 Team.
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
#include <condition_variable>
#include <mutex>

#include <drivers/graphics/arena.h>

namespace eka2l1 {
    class graphics_driver;
}

namespace eka2l1::drivers {
    /**
     * \brief Represent a command for driver.
     */
    struct command {
        std::uint32_t opcode_;
        std::uint64_t data_[10];
        int *status_;

        explicit command()
            : opcode_(0)
            , data_()
            , status_(nullptr) {
        }

        explicit command(const std::uint16_t opcode, int *status = nullptr)
            : opcode_(opcode)
            , data_()
            , status_(status) {
        }
    };

    // Forward-declared: see drivers/graphics/arena.h
    class arena;

    /// Type-erased arena release callback. Called by the render thread after
    /// dispatching an arena-backed command list. The pool tag is an opaque
    /// pointer to an arena_pool<N> instance; the function knows the N and can
    /// call pool->release(arena).
    using arena_release_func = void (*)(void *pool_tag, arena *a);

    /**
     * \brief A linked list of command.
     *
     * Optionally backed by an arena allocator. When \c arena_ is set,
     * command storage and associated variable data are allocated from
     * the arena and must NOT be individually freed — the arena is
     * recycled as a whole via \c release_arena().
     */
    struct command_list {
        command *base_;

        std::size_t size_;
        std::size_t max_cap_;
        arena *arena_;

        /// When non-null, called to return the arena to its owning pool.
        /// If null, the global driver pools are tried (backward compat).
        void *arena_pool_tag_ = nullptr;
        arena_release_func arena_release_fn_ = nullptr;

        explicit command_list(std::size_t max_cap = 0)
            : base_(nullptr)
            , size_(0)
            , max_cap_(max_cap)
            , arena_(nullptr) {
        }

        bool empty() const {
            return (size_ == 0);
        }

        /** \brief True if this list is backed by an arena. */
        bool is_arena_backed() const {
            return (arena_ != nullptr);
        }

        /** \brief Set the backing arena. Must be called before renew(). */
        void set_arena(arena *a) {
            arena_ = a;
            arena_pool_tag_ = nullptr;
            arena_release_fn_ = nullptr;
        }

        /** 
         * \brief Set the backing arena with a release callback for a
         *        private (non-global) pool.
         */
        void set_arena(arena *a, void *pool_tag, arena_release_func fn) {
            arena_ = a;
            arena_pool_tag_ = pool_tag;
            arena_release_fn_ = fn;
        }

        /** \brief Return the arena to its owning pool, if any. */
        void release_arena() {
            if (arena_ && arena_release_fn_) {
                arena_release_fn_(arena_pool_tag_, arena_);
                arena_ = nullptr;
            }
        }

        command *retrieve_next() {
            if (!max_cap_) {
                return nullptr;
            }

            if (!base_) {
                renew();
            }

            command *res = base_ + size_;
            res->status_ = nullptr;

            size_++;

            return res;
        }

        void renew() {
            if (max_cap_ == 0) {
                return;
            }

            if (arena_) {
                // Allocate command array from the arena. Falls back to
                // heap if the arena is full (should not happen with
                // properly sized arenas).
                const std::size_t cmd_size = max_cap_ * sizeof(command);
                base_ = static_cast<command *>(arena_->allocate(cmd_size, alignof(command)));
                if (!base_) {
                    // Arena overflow — rare fallback to heap
                    base_ = new command[max_cap_];
                }
            } else {
                // NOTE: After command all iterated, the base will be deleted.
                // No memory leak!
                base_ = new command[max_cap_];
            }
            size_ = 0;
        }
    };

    class driver {
    public:
        std::mutex mut_;
        std::condition_variable cond_;

        virtual ~driver() {}
        virtual void run() = 0;
        virtual void abort() = 0;
        virtual bool aborted() const {
            return false;
        }

        virtual void wait_for(int *status) {
            std::unique_lock<std::mutex> ulock(mut_);

            if (*status == 0) {
                return;
            }

            cond_.wait(ulock, [&]() { return (*status != -100) || aborted(); });
        }

        void finish(int *status, const int code) {
            if (status) {
                std::unique_lock<std::mutex> ulock(mut_);

                *status = code;
                cond_.notify_all();
            }
        }
    };
}
