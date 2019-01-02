/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <epoc/services/window/bitmap.h> 
#include <epoc/epoc.h>
#include <epoc/page_table.h>
#include <epoc/kernel/libmanager.h>
#include <arm/arm_interface.h>

#include <epoc/kernel.h>

eka2l1::ptr<std::uint32_t> jump_chunk_addr = 0;
eka2l1::ptr<std::uint32_t> jump_chunk_thumb_addr = 0;

std::uint32_t jump_chunk_size = 0;
std::uint32_t jump_chunk_thumb_size = 0;

namespace eka2l1 {
    void supply_jump_arg(eka2l1::memory_system *mem, address jump_addr, address r0,
        bool thumb) {
        std::uint32_t *jump_arg_ptr = nullptr;

        if (thumb) {
            jump_arg_ptr = ptr<std::uint32_t>(jump_chunk_addr.ptr_address() + jump_chunk_size).get(mem);
        } else {
            jump_arg_ptr = ptr<std::uint32_t>(jump_chunk_thumb_addr.ptr_address() + jump_chunk_thumb_size).get(mem);
        }

        *jump_arg_ptr = r0;
        *(jump_arg_ptr + 1) = jump_addr;
    }

    void generate_jump_chunk(eka2l1::memory_system *mem) {
        jump_chunk_addr = mem->chunk_range
            (ram_code_addr, rom, 0, 0x1000, 0x1000, prot::read_write_exec).cast<std::uint32_t>();

        std::uint32_t *jump_chunk = jump_chunk_addr.get(mem);
        
        // MOV R0, fbs
        // MOV R14, jump addr
        // BLX fbs_get_data_addr
        
        // After that, data will be stored in R0
        // Everything should be restored back to original context

        *jump_chunk = 0xE59FF004;           // LDR R0, [PC, #4]
        *(jump_chunk + 1) = 0xE59FE008;     // LDR R14, [PC, #8]
        *(jump_chunk + 2) = 0xE12FFF3E;     // BLX R14

        jump_chunk_size = 12;

        // Now it's the place that we supply the fbs pointer and fbs get data address

        // Leave these two alone, generate code for thumb
        jump_chunk += 2;

        jump_chunk_thumb_addr = jump_chunk_addr + jump_chunk_size + 8;

        std::uint16_t *jump_chunk_thumb = reinterpret_cast<std::uint16_t*>(jump_chunk);

        *jump_chunk_thumb = 0x4801;           // LDR R0, [PC, #4]
        *(jump_chunk_thumb + 1) = 0x4A02;     // LDR R1, [PC, #8]
        *(jump_chunk_thumb + 2) = 0x4788;     // BLX R1

        // Leave alone again
        jump_chunk_thumb_size = 6;
    }

    eka2l1::ptr<std::uint32_t> get_fbs_data(eka2l1::system *sys, eka2l1::process_ptr call_pr,
        eka2l1::ptr<fbs_bitmap> bitmap) {
        memory_system *mem = sys->get_memory_system();

        bool switch_page = false;

        auto current_page_table = mem->get_current_page_table();

        if (current_page_table != &(call_pr->get_page_table())) {
            switch_page = true;
        }

        if (switch_page) {
            mem->set_current_page_table(call_pr->get_page_table());
        }

        arm::jitter &cpu = sys->get_cpu();

        // TODO: Not hardcode this SID. This is the SID for the function CFbsBitmap::DataAddress()
        address get_addr_addr = sys->get_lib_manager()->get_export_addr(3835946576);
        bool thumb = cpu->is_thumb_mode();

        // Generate the jump chunk
        if (!jump_chunk_addr || !jump_chunk_thumb_addr) {
            generate_jump_chunk(mem);
        }

        supply_jump_arg(mem, get_addr_addr, bitmap.ptr_address(), thumb);
        
        // Save the context. Later we will only restore R0, R1, R2, R3 and R14 (if ARM mode)
        arm::arm_interface::thread_context ctx;
        cpu->save_context(ctx);

        if (thumb) {
            cpu->imb_range(jump_chunk_thumb_addr.ptr_address(), jump_chunk_thumb_size);
        } else {
            cpu->imb_range(jump_chunk_addr.ptr_address(), jump_chunk_size);
        }

        address data_addr = cpu->get_reg(0);

        cpu->set_reg(0, ctx.cpu_registers[0]);
        cpu->set_reg(1, ctx.cpu_registers[1]);
        cpu->set_reg(2, ctx.cpu_registers[2]);
        cpu->set_reg(3, ctx.cpu_registers[3]);

        if (!thumb) {
            cpu->set_reg(14, ctx.cpu_registers[14]);
        }

        if (switch_page) {
            mem->set_current_page_table(*current_page_table);
        }

        return data_addr;
    }
}