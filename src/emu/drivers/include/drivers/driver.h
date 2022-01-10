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

#include <common/queue.h>
#include <condition_variable>

namespace eka2l1::drivers {
    static constexpr std::uint32_t MAX_COMMAND_DATA_SIZE = 80;

    /**
     * \brief Represent a command for driver.
     */
    struct command {
        std::uint16_t opcode_;
        std::uint8_t data_[MAX_COMMAND_DATA_SIZE];

        command *next_;
        int *status_;

        explicit command()
            : opcode_(0)
            , data_()
            , next_(nullptr)
            , status_(nullptr) {
        }

        explicit command(const std::uint16_t opcode, int *status = nullptr)
            : opcode_(opcode)
            , data_()
            , next_(nullptr)
            , status_(status) {
        }
    };

    struct command_helper {
        std::uint16_t cursor_;
        command *todo_;

        explicit command_helper(command *todo)
            : cursor_(0)
            , todo_(todo) {
        }

        bool push(const std::uint8_t *data, const std::uint16_t data_size) {
            if (cursor_ + data_size > MAX_COMMAND_DATA_SIZE) {
                // Data full, abort
                return false;
            }

            std::copy(data, data + data_size, todo_->data_ + cursor_);
            cursor_ += data_size;

            return true;
        }

        bool pop(std::uint8_t *dest, const std::uint16_t dest_size) {
            if (cursor_ + dest_size > MAX_COMMAND_DATA_SIZE) {
                // Not possible to pop, abort
                return false;
            }

            std::copy(todo_->data_ + cursor_, todo_->data_ + cursor_ + dest_size, dest);

            cursor_ += dest_size;
            return true;
        }

        template <typename T>
        bool push(const T &data) {
            return push(reinterpret_cast<const std::uint8_t *>(&data), sizeof(T));
        }

        template <typename T>
        bool pop(T &dest) {
            return pop(reinterpret_cast<std::uint8_t *>(&dest), sizeof(T));
        }

        template <typename T>
        void finish(T *drv, const int code) {
            if (todo_->status_) {
                *todo_->status_ = code;
                drv->cond_.notify_all();
            }
        }

        bool push_string(const std::u16string &data) {
            // Alloc memory for the string
            std::uint16_t length = static_cast<std::uint16_t>(data.length());

            if (!push(length)) {
                return false;
            }

            char16_t *dat = new char16_t[length];
            std::copy(data.data(), data.data() + length, dat);

            if (!push(dat)) {
                return false;
            }

            return true;
        }

        bool pop_string(std::u16string &dat) {
            std::uint16_t length = 0;
            if (!pop(length)) {
                return false;
            }

            dat.resize(length);
            char16_t *ptr = nullptr;

            if (!pop(ptr)) {
                return false;
            }

            std::copy(ptr, ptr + length, &dat[0]);
            delete ptr;

            return true;
        }
    };

    template <typename Head, typename... Args>
    inline void push_arguments(command_helper &helper, Head arg1, Args... args) {
        helper.push(arg1);

        if constexpr (sizeof...(Args) > 0) {
            push_arguments(helper, args...);
        }
    }

    template <typename... Args>
    command *make_command(const std::uint16_t opcode, int *status, Args... arguments) {
        command *cmd = new command(opcode, status);
        command_helper helper(cmd);

        if constexpr (sizeof...(Args) > 0)
            push_arguments(helper, arguments...);

        return cmd;
    }

    /**
     * \brief A linked list of command.
     */
    struct command_list {
        command *first_;
        command *last_;

        explicit command_list()
            : first_(nullptr) {
        }

        bool empty() const {
            return !first_;
        }

        void add(command *cmd_) {
            if (first_ == nullptr) {
                first_ = cmd_;
                last_ = cmd_;

                return;
            }

            last_->next_ = cmd_;
            last_ = cmd_;
        }
    };

    class driver {
    public:
        std::mutex mut_;
        std::condition_variable cond_;

        virtual ~driver() {}
        virtual void run() = 0;
        virtual void abort() = 0;

        void wake_clients() {
            cond_.notify_all();
        }

        void wait_for(int *status) {
            if (*status == 0) {
                return;
            }

            std::unique_lock<std::mutex> ulock(mut_);
            cond_.wait(ulock, [&]() { return *status != -100; });
        }
    };
}
