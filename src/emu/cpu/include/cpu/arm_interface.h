/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <array>
#include <memory>
#include <functional>

#include <common/types.h>

namespace eka2l1::arm {
    enum exception_type {
        exception_type_unk = 0,
        exception_type_access_violation_read = 1,
        exception_type_access_violation_write = 2,
        exception_type_breakpoint = 3,
        exception_type_undefined_inst = 4,
        exception_type_unpredictable = 5
    };

    using address = std::uint32_t;
    using memory_operation_8bit_func = std::function<bool(address, std::uint8_t*)>;
    using memory_operation_16bit_func = std::function<bool(address, std::uint16_t*)>;
    using memory_operation_32bit_func = std::function<bool(address, std::uint32_t*)>;
    using memory_operation_64bit_func = std::function<bool(address, std::uint64_t*)>;

    using system_call_handler_func = std::function<void(const std::uint32_t)>;
    using handle_exception_func = std::function<void(exception_type, const std::uint32_t)>;

    class core {
    public:
        memory_operation_8bit_func read_8bit;
        memory_operation_8bit_func write_8bit;

        memory_operation_16bit_func read_16bit;
        memory_operation_16bit_func write_16bit;

        memory_operation_32bit_func read_32bit;
        memory_operation_32bit_func write_32bit;

        memory_operation_64bit_func read_64bit;
        memory_operation_64bit_func write_64bit;

        system_call_handler_func system_call_handler;
        handle_exception_func exception_handler;

        /**
         *  Stores register value and some pointer of the CPU.
        */
        struct thread_context {
            std::array<uint32_t, 31> cpu_registers;
            std::uint32_t sp;
            std::uint32_t pc;
            std::uint32_t lr;
            std::uint32_t cpsr;
            std::array<uint64_t, 32> fpu_registers;
            std::uint32_t fpscr;
            std::uint32_t wrwr;
        };

        virtual ~core() {}

        /*! Run the CPU */
        virtual void run(const std::uint32_t instruction_count) = 0;

        /*! Stop the CPU */
        virtual void stop() = 0;

        /*! Step the CPU. Each step execute one instruction. */
        virtual void step() = 0;

        /*! Get a specific ARM Rx register. Range from r0 to r15 */
        virtual uint32_t get_reg(size_t idx) = 0;

        /*! Get the stack pointer */
        virtual uint32_t get_sp() = 0;

        /*! Get the program counter */
        virtual uint32_t get_pc() = 0;

        /*! Get the VFP */
        virtual uint32_t get_vfp(size_t idx) = 0;

        /*! Set a Rx register */
        virtual void set_reg(size_t idx, uint32_t val) = 0;

        virtual void set_cpsr(uint32_t val) = 0;

        /*! Set program counter */
        virtual void set_pc(uint32_t val) = 0;

        /*! Set LR */
        virtual void set_lr(uint32_t val) = 0;

        /*! Set stack pointer */
        virtual void set_sp(uint32_t val) = 0;
        virtual void set_vfp(size_t idx, uint32_t val) = 0;
        virtual uint32_t get_lr() = 0;
        virtual void set_entry_point(address ep) = 0;
        virtual address get_entry_point() = 0;
        virtual uint32_t get_cpsr() = 0;

        /*! Save thread context from JIT. */
        virtual void save_context(thread_context &ctx) = 0;

        /*! Load thread context to JIT. */
        virtual void load_context(const thread_context &ctx) = 0;

        /*! Same as set SP */
        virtual void set_stack_top(address addr) = 0;

        /*! Same as get SP */
        virtual address get_stack_top() = 0;

        virtual void prepare_rescheduling() = 0;

        virtual bool is_thumb_mode() = 0;

        virtual void page_table_changed() = 0;

        virtual void map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) = 0;

        virtual void unmap_memory(address addr, size_t size) = 0;

        virtual void clear_instruction_cache() = 0;

        virtual void imb_range(address addr, std::size_t size) = 0;

        virtual bool should_clear_old_memory_map() const {
            return true;
        }

        virtual std::uint32_t get_num_instruction_executed() = 0;
    };
}
