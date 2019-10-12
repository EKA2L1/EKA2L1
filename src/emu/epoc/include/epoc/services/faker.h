/*
 * Copyright (c) 2019 EKA2L1 Team
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

#pragma once

#include <common/allocator.h>
#include <epoc/ptr.h>

#include <memory>
#include <string>
#include <vector>

// Foward declaration
namespace eka2l1 {
    class kernel_system;
    
    namespace kernel {
        class process;
        class chunk;
        class thread;
    }

    namespace hle {
        class lib_manager;
    }

    using chunk_ptr = std::shared_ptr<kernel::chunk>;
}

namespace eka2l1::service {
    class faker;

    /**
     * \brief Allow manipulation of raw Symbian's native code.
     * 
     * This allow native code to run as a normal Symbian process, also provide a callback and
     * chain like mechanism when a native code function are done executing.
     * 
     * Because this is revolving around Symbian, not everything will work out of the box and be
     * happy about this.
     * 
     * For example, memory allocation will be a burden because this fake thread doesn't have a heap
     * available and a heap allocator with it. Also the exception mechanism (trap) will fail. Which
     * is why this class also does some workaround with specific kernel version in order to make
     * Symbian thought this is like a real thread running.
     */
    class faker {
    private:
        kernel::process *process_;
        chunk_ptr        control_;

        address lr_addr_;
        address data_offset_;

        std::unique_ptr<common::block_allocator> allocator_;
        std::vector<std::uint8_t*> free_lists_;

    public:
        struct chain {
            typedef void (*chain_func)(faker *self, void *userdata);

            chain       *next_;
            faker       *daddy_;

            std::vector<std::uint8_t*> free_lists_;

            enum class chain_type {
                unk,
                raw_code,
                hook
            };
            
            struct {
                chain_type type_;

                union chain_data {
                    struct {        
                        chain_func  func_;
                        void        *userdata_;
                    };

                    struct {
                        std::uint32_t raw_func_addr_;
                        std::uint32_t raw_func_args_[4];
                    };
                } data_;
            };

            explicit chain(faker *daddy);
            chain *then(void *userdata, chain_func func);

            /**
             * \brief Continue the chain with new native code task after the current task has been done.
             * 
             * \param raw_func_addr The address of the function in emulated space address.
             * 
             * \returns Pointer to start of next hook.
             */
            chain *then(eka2l1::ptr<void> raw_func_addr);

            /**
             * \brief Continue the chain with new native code task after the current task has been done.
             * 
             * \param raw_func_addr The address of the function in emulated space address.
             * \param arg1          The first argument of the chain.
             * 
             * \returns Pointer to start of next hook.
             */
            chain *then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1);
            chain *then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1, const std::uint32_t arg2);
        };

    private:
        chain *initial_;

        eka2l1::address trampoline_address() const;

        /**
         * \brief Initialise the fake process.
         * 
         * \param kern Kernel system.
         * \param mngr The library manager system.
         * \param name UTF-16 string contains name of process.
         * 
         * \returns True on success.
         * 
         * \internal
         */
        bool initialise(kernel_system *kern, hle::lib_manager *mngr, const std::string &name,
            const std::uint32_t uid);

    public:
        explicit faker(kernel_system *kern, hle::lib_manager *mngr, const std::string &name,
            const std::uint32_t uid);

        // ==================== UTILITIES =======================
        chain *then(void *userdata, chain::chain_func func);
        
        chain *then(eka2l1::ptr<void> raw_func_addr);
        chain *then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1);
        chain *then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1, const std::uint32_t arg2);

        bool walk();

        std::uint32_t get_native_return_value() const;

        /**
         * \brief   Alloc a temporary space for an argument.
         * 
         * The argument will be freed on the next raw native call.
         * 
         * \param   size        Size of data to alloc.
         * \param   pointer     Optional pointer to a variable which will contains host
         *                      pointer to the variable.
         * 
         * \returns Guest address of the argument
         */
        eka2l1::address new_temporary_argument_with_size(const std::uint32_t size, std::uint8_t **pointer = nullptr);

        template <typename T>
        eka2l1::address new_temp_arg(T **pointer = nullptr) {
            return new_temporary_argument_with_size(sizeof(T), reinterpret_cast<std::uint8_t**>(pointer));
        }

        // Use this function only in a hook
        std::uint8_t *get_last_temporary_argument_impl(const int number);

        // Use this function only in a hook
        template <typename T>
        T *last_temp_arg(const int number) {
            return reinterpret_cast<T*>(get_last_temporary_argument_impl(number));
        }

        // ==================== GETTER =========================
        kernel::process *process() {
            return process_;
        }
        
        chain *initial() {
            return initial_;
        }
        
        kernel::thread *main_thread();
    };
}