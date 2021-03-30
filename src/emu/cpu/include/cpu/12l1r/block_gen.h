/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/armemitter.h>

#include <cpu/12l1r/block_cache.h>
#include <cpu/arm_interface.h>
#include <cpu/dyncom/arm_dyncom.h>

#include <cstdint>
#include <map>

namespace eka2l1::arm {
    class r12l1_core;
}

namespace eka2l1::arm::r12l1 {
    struct core_state;
    class reg_cache;
    class visit_session;
    class exclusive_monitor;

    struct fast_dispatch_entry {
        std::uint32_t addr_;
        std::uint32_t asid_;

        void *code_;
        std::uint32_t padding_;
    };

    // In fast dispatch we favour the lower end of the address. Since it is not the common case
    // Symbian code jumps to other memory region, so the upper part may not be much count.
    // Here we take account of bits[4:20] of the block address.
    static constexpr std::uint32_t FAST_DISPATCH_ENTRY_ADDR_SHIFT = 4;
    static constexpr std::uint32_t FAST_DISPATCH_ENTRY_MASK = 0xFFFF;
    static constexpr std::uint32_t FAST_DISPATCH_ENTRY_COUNT = 0x10000;

    class dashixiong_block : public common::armgen::armx_codeblock {
    private:
        std::multimap<translated_block::hash_type, translated_block *> link_to_;
        std::array<fast_dispatch_entry, FAST_DISPATCH_ENTRY_COUNT> fast_dispatches_;

        block_cache cache_;

        void *dispatch_func_;
        const void *dispatch_ent_for_block_;
        const void *fast_dispatch_ent_;

        std::uint32_t flags_;

        r12l1_core *parent_;

        std::unique_ptr<dyncom_core> interpreter_;

    protected:
        void assemble_control_funcs();
        translated_block *start_new_block(const vaddress addr, const asid aid);

    public:
        enum {
            FLAG_GENERATE_FUZZ = 1 << 0,
            FLAG_ENABLE_FUZZ = 1 << 1,
            FLAG_FUZZ_LAST_SYSCALL = 1 << 2
        };

        explicit dashixiong_block(r12l1_core *parent);
        void enter_dispatch(core_state *cstate);

        translated_block *compile_new_block(core_state *state, const vaddress addr);
        translated_block *get_block(const vaddress addr, const asid aid);

        void emit_block_links(translated_block *block);
        void emit_return_to_dispatch(translated_block *block, const bool fast_hint);
        void edit_block_links(translated_block *dest, bool unlink = false);

        void emit_pc_flush(const address current_pc);
        void emit_pc_write_exchange(common::armgen::arm_reg pc_reg);
        void emit_pc_write(common::armgen::arm_reg pc_reg);
        void emit_cycles_count_add(const std::uint32_t num);
        void emit_cpsr_save();
        void emit_cpsr_load();
        void emit_cycles_count_save();

        void clear_fast_dispatch();

        void flush_range(const vaddress start, const vaddress end, const asid aid);
        void flush_all();

        void raise_guest_exception(const exception_type exc, const std::uint32_t usrdata);
        void raise_system_call(const std::uint32_t num);

        std::uint8_t read_byte(const vaddress addr);
        std::uint16_t read_word(const vaddress addr);
        std::uint32_t read_dword(const vaddress addr);
        std::uint64_t read_qword(const vaddress addr);

        void write_byte(const vaddress addr, std::uint8_t dat);
        void write_word(const vaddress addr, std::uint16_t dat);
        void write_dword(const vaddress addr, std::uint32_t dat);
        void write_qword(const vaddress addr, std::uint64_t dat);

        std::uint8_t read_and_mark_byte(const vaddress addr);
        std::uint16_t read_and_mark_word(const vaddress addr);
        std::uint32_t read_and_mark_dword(const vaddress addr);
        std::uint64_t read_and_mark_qword(const vaddress addr);

        bool write_ex_byte(const vaddress addr, const std::uint8_t val);
        bool write_ex_word(const vaddress addr, const std::uint16_t val);
        bool write_ex_dword(const vaddress addr, const std::uint32_t val);
        bool write_ex_qword(const vaddress addr, const std::uint64_t val);

        void fuzz_start();
        bool fuzz_execute();
        void fuzz_compare(core_state *state);
        void fuzz_end();

        std::uint32_t config_flags() const {
            return flags_;
        }
    };
}