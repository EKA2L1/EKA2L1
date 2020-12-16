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
    class core;

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
    using memory_operation_ew_8bit_func = std::function<std::int32_t(address, std::uint8_t, std::uint8_t)>;
    using memory_operation_ew_16bit_func = std::function<std::int32_t(address, std::uint16_t, std::uint16_t)>;
    using memory_operation_ew_32bit_func = std::function<std::int32_t(address, std::uint32_t, std::uint32_t)>;
    using memory_operation_ew_64bit_func = std::function<std::int32_t(address, std::uint64_t, std::uint64_t)>;

    using memory_read_with_core_8bit_func = std::function<bool(core*, address, std::uint8_t*)>;
    using memory_read_with_core_16bit_func = std::function<bool(core*, address, std::uint16_t*)>;
    using memory_read_with_core_32bit_func = std::function<bool(core*, address, std::uint32_t*)>;
    using memory_read_with_core_64bit_func = std::function<bool(core*, address, std::uint64_t*)>;

    using memory_write_exclusive_with_core_8bit_func = std::function<std::int32_t(core*, address, std::uint8_t, std::uint8_t)>;
    using memory_write_exclusive_with_core_16bit_func = std::function<std::int32_t(core*, address, std::uint16_t, std::uint16_t)>;
    using memory_write_exclusive_with_core_32bit_func = std::function<std::int32_t(core*, address, std::uint32_t, std::uint32_t)>;
    using memory_write_exclusive_with_core_64bit_func = std::function<std::int32_t(core*, address, std::uint64_t, std::uint64_t)>;

    using system_call_handler_func = std::function<void(const std::uint32_t)>;
    using handle_exception_func = std::function<void(exception_type, const std::uint32_t)>;

    class core;

    class exclusive_monitor {
    public:
        memory_read_with_core_8bit_func read_8bit;
        memory_write_exclusive_with_core_8bit_func write_8bit;

        memory_read_with_core_16bit_func read_16bit;
        memory_write_exclusive_with_core_16bit_func write_16bit;

        memory_read_with_core_32bit_func read_32bit;
        memory_write_exclusive_with_core_32bit_func write_32bit;

        memory_read_with_core_64bit_func read_64bit;
        memory_write_exclusive_with_core_64bit_func write_64bit;

        virtual ~exclusive_monitor() {}

        virtual std::uint8_t exclusive_read8(core *cc, address vaddr) = 0;
        virtual std::uint16_t exclusive_read16(core *cc, address vaddr) = 0;
        virtual std::uint32_t exclusive_read32(core *cc, address vaddr) = 0;
        virtual std::uint64_t exclusive_read64(core *cc, address vaddr) = 0;
        virtual void clear_exclusive() = 0;

        virtual bool exclusive_write8(core *cc, address vaddr, std::uint8_t value) = 0;
        virtual bool exclusive_write16(core *cc, address vaddr, std::uint16_t value) = 0;
        virtual bool exclusive_write32(core *cc, address vaddr, std::uint32_t value) = 0;
        virtual bool exclusive_write64(core *cc, address vaddr, std::uint64_t value) = 0;
    };

    class core {
    private:
        std::size_t core_num_ = 0;

    public:
        memory_operation_8bit_func read_8bit;
        memory_operation_8bit_func write_8bit;

        memory_operation_16bit_func read_16bit;
        memory_operation_16bit_func write_16bit;

        memory_operation_32bit_func read_32bit;
        memory_operation_32bit_func write_32bit;

        memory_operation_64bit_func read_64bit;
        memory_operation_64bit_func write_64bit;

        memory_operation_ew_8bit_func exclusive_write_8bit;
        memory_operation_ew_16bit_func exclusive_write_16bit;
        memory_operation_ew_32bit_func exclusive_write_32bit;
        memory_operation_ew_64bit_func exclusive_write_64bit;

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

        std::size_t core_number() const {
            return core_num_;
        }

        void set_core_number(const std::size_t num) {
            core_num_ = num;
        }

        virtual void run(const std::uint32_t instruction_count) = 0;
        virtual void stop() = 0;
        virtual void step() = 0;
        virtual uint32_t get_reg(size_t idx) = 0;
        virtual uint32_t get_sp() = 0;
        virtual uint32_t get_pc() = 0;
        virtual uint32_t get_vfp(size_t idx) = 0;
        virtual void set_reg(size_t idx, uint32_t val) = 0;

        virtual void set_cpsr(uint32_t val) = 0;
        virtual void set_pc(uint32_t val) = 0;
        virtual void set_lr(uint32_t val) = 0;

        virtual void set_sp(uint32_t val) = 0;
        virtual void set_vfp(size_t idx, uint32_t val) = 0;
        virtual uint32_t get_lr() = 0;
        virtual uint32_t get_cpsr() = 0;

        virtual void save_context(thread_context &ctx) = 0;
        virtual void load_context(const thread_context &ctx) = 0;

        virtual bool is_thumb_mode() = 0;

        virtual void map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) = 0;
        virtual void unmap_memory(address addr, size_t size) = 0;

        virtual void clear_instruction_cache() = 0;
        virtual void imb_range(address addr, std::size_t size) = 0;

        virtual bool should_clear_old_memory_map() const {
            return true;
        }

        virtual void set_asid(std::uint8_t num) = 0;
        virtual std::uint8_t get_asid() const = 0;
        virtual std::uint8_t get_max_asid_available() const = 0;

        virtual std::uint32_t get_num_instruction_executed() = 0;
    };
}
