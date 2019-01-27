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

#include <arm/arm_interface.h>
#include <epoc/epoc.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/page_table.h>
#include <epoc/services/window/bitmap.h>

#include <common/log.h>
#include <epoc/kernel.h>

namespace eka2l1 {
    void bitmap_manipulator::supply_jump_arg(eka2l1::memory_system *mem, address jump_addr, address r0,
        bool thumb) {
        std::uint32_t *jump_arg_ptr = nullptr;

        if (thumb) {
            jump_arg_ptr = ptr<std::uint32_t>(jump_arm_one_arg.code_ptr.ptr_address() + jump_arm_one_arg.chunk_size).get(mem);
        } else {
            jump_arg_ptr = ptr<std::uint32_t>(jump_thumb_one_arg.code_ptr.ptr_address() + jump_thumb_one_arg.chunk_size).get(mem);
        }

        *jump_arg_ptr = r0;
        *(jump_arg_ptr + 1) = jump_addr;
    }

    void bitmap_manipulator::supply_jump_arg(eka2l1::memory_system *mem, address jump_addr, address r0,
        address r1, bool thumb) {
        std::uint32_t *jump_arg_ptr = nullptr;

        if (thumb) {
            jump_arg_ptr = ptr<std::uint32_t>(jump_arm_two_arg.code_ptr.ptr_address() + jump_arm_two_arg.chunk_size).get(mem);
        } else {
            jump_arg_ptr = ptr<std::uint32_t>(jump_thumb_one_arg.code_ptr.ptr_address() + jump_thumb_two_arg.chunk_size).get(mem);
        }

        *jump_arg_ptr = r0;
        *(jump_arg_ptr + 1) = r1;
        *(jump_arg_ptr + 2) = jump_addr;
    }

    void bitmap_manipulator::generate_jump_chunk(eka2l1::memory_system *mem) {
        jump_arm_one_arg.code_ptr = mem->chunk_range(ram_code_addr, rom, 0, 0x1000, 0x1000, prot::read_write_exec).cast<std::uint32_t>();

        std::uint32_t *jump_chunk = jump_arm_one_arg.code_ptr.get(mem);

        // MOV R0, fbs
        // MOV R14, jump addr
        // BLX fbs_get_data_addr

        // After that, data will be stored in R0
        // Everything should be restored back to original context

        *jump_chunk = 0xE59FF004; // LDR R0, [PC, #4]
        *(jump_chunk + 1) = 0xE59FE008; // LDR R14, [PC, #8]
        *(jump_chunk + 2) = 0xE12FFF3E; // BLX R14

        jump_arm_one_arg.chunk_size = 12;

        // Now it's the place that we supply the fbs pointer and fbs get data address

        // Leave these two alone, generate code for thumb
        jump_chunk += 2;

        jump_thumb_one_arg.code_ptr = jump_arm_one_arg.code_ptr + jump_arm_one_arg.chunk_size + 8;

        std::uint16_t *jump_chunk_thumb = reinterpret_cast<std::uint16_t *>(jump_chunk);

        *jump_chunk_thumb = 0x4801; // LDR R0, [PC, #4]
        *(jump_chunk_thumb + 1) = 0x4A02; // LDR R1, [PC, #8]
        *(jump_chunk_thumb + 2) = 0x4788; // BLX R1

        // Leave alone again
        jump_thumb_two_arg.chunk_size = 6;

        jump_chunk = reinterpret_cast<std::uint32_t *>(jump_chunk_thumb + 4);
        jump_arm_two_arg.code_ptr = jump_thumb_one_arg.code_ptr.ptr_address() + jump_thumb_one_arg.chunk_size
            + 8;

        *jump_chunk = 0xE59FF004; // LDR R0, [PC, #4]
        *(jump_chunk + 1) = 0xE59F1008; // LDR R1, [PC, #8]
        *(jump_chunk + 2) = 0xE59FE00C; // LDR R14, [PC, 0xC]
        *(jump_chunk + 3) = 0xE12FFF3E; // BLX R14

        jump_arm_two_arg.chunk_size = 16;

        jump_chunk_thumb = reinterpret_cast<std::uint16_t *>(jump_chunk + 3);
        jump_thumb_two_arg.code_ptr = jump_arm_two_arg.code_ptr.ptr_address() + jump_arm_two_arg.chunk_size
            + 12;

        *jump_chunk_thumb = 0x4801; // LDR R0, [PC, #4]
        *(jump_chunk_thumb + 1) = 0x4A02; // LDR R1, [PC, #8]
        *(jump_chunk_thumb + 2) = 0x4A03; // LDR R2, [PC, #12]
        *(jump_chunk_thumb + 2) = 0x4790; // BLX R2

        jump_thumb_two_arg.chunk_size = 12;
    }

    bitmap_manipulator::bitmap_manipulator(eka2l1::system *sys)
        : sys(sys) {
        fbscli = sys->get_lib_manager()->load_romimg(u"fbscli.dll", false);

        // https://github.com/SymbianSource/oss.FCL.sf.os.graphics/blob/ff133bc50e6158bfb08cc093b0f0055321dcde99/fbs/fontandbitmapserver/eabi/FBSCLI2U.DEF
        // _ZN10CFbsBitmap9DuplicateEi @ 30 NONAME
        duplicate_func_addr = fbscli->exports[29];

        // _ZNK10CFbsBitmap11DataAddressEv @ 108 NONAME
        get_data_func_addr = fbscli->exports[107];

        // _ZN10CFbsBitmap5ResetEv @ 25 NONAME
        reset_func_addr = fbscli->exports[24];

        bool is_eka1 = (sys->get_symbian_version_use() <= epocver::epoc93);

        scratch = sys->get_memory_system()->chunk_range(is_eka1 ? shared_data_eka1 : shared_data, is_eka1 ? shared_data_end_eka1 : ram_code_addr, 0,
                                              sizeof(fbs_bitmap), 0x1000, prot::read_write)
                      .cast<fbs_bitmap>();

        scratch_host = scratch.get(sys->get_memory_system());
    }

    address bitmap_manipulator::do_call(eka2l1::process_ptr &call_pr, address jump_addr, std::uint32_t r0) {
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
        bool thumb = cpu->is_thumb_mode();

        // Generate the jump chunk
        if (!gened) {
            generate_jump_chunk(mem);
            gened = true;
        }

        supply_jump_arg(mem, jump_addr, r0, thumb);

        // Save the context. Later we will only restore R0, R1, R2, R3 and R14 (if ARM mode)
        std::uint32_t lr0 = cpu->get_reg(0);
        std::uint32_t lr1 = cpu->get_reg(1);
        std::uint32_t lr2 = cpu->get_reg(2);
        std::uint32_t lr3 = cpu->get_reg(3);
        std::uint32_t lr14 = cpu->get_reg(14);

        if (thumb) {
            cpu->imb_range(jump_arm_one_arg.code_ptr.ptr_address(), jump_arm_one_arg.chunk_size);
        } else {
            cpu->imb_range(jump_thumb_one_arg.code_ptr.ptr_address(), jump_thumb_one_arg.chunk_size);
        }

        address data_addr = cpu->get_reg(0);

        cpu->set_reg(0, lr0);
        cpu->set_reg(1, lr1);
        cpu->set_reg(2, lr2);
        cpu->set_reg(3, lr3);

        if (!thumb) {
            cpu->set_reg(14, lr14);
        }

        if (switch_page) {
            mem->set_current_page_table(*current_page_table);
        }

        return data_addr;
    }

    address bitmap_manipulator::do_call(eka2l1::process_ptr &call_pr, address jump_addr, std::uint32_t r0,
        std::uint32_t r1) {
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
        bool thumb = cpu->is_thumb_mode();

        // Generate the jump chunk
        if (!gened) {
            generate_jump_chunk(mem);
            gened = true;
        }

        supply_jump_arg(mem, jump_addr, r0, r1, thumb);

        // Save the context. Later we will only restore R0, R1, R2, R3 and R14 (if ARM mode)
        std::uint32_t lr0 = cpu->get_reg(0);
        std::uint32_t lr1 = cpu->get_reg(1);
        std::uint32_t lr2 = cpu->get_reg(2);
        std::uint32_t lr3 = cpu->get_reg(3);
        std::uint32_t lr14 = cpu->get_reg(14);

        if (thumb) {
            cpu->imb_range(jump_arm_two_arg.code_ptr.ptr_address(), jump_arm_two_arg.chunk_size);
        } else {
            cpu->imb_range(jump_thumb_two_arg.code_ptr.ptr_address(), jump_thumb_two_arg.chunk_size);
        }

        address data_addr = cpu->get_reg(0);

        cpu->set_reg(0, lr0);
        cpu->set_reg(1, lr1);
        cpu->set_reg(2, lr2);
        cpu->set_reg(3, lr3);

        if (!thumb) {
            cpu->set_reg(14, lr14);
        }

        if (switch_page) {
            mem->set_current_page_table(*current_page_table);
        }

        return data_addr;
    }

    eka2l1::ptr<std::uint32_t> bitmap_manipulator::get_fbs_data(eka2l1::process_ptr &call_pr,
        eka2l1::ptr<fbs_bitmap> bitmap) {
        return do_call(call_pr, get_data_func_addr, bitmap.ptr_address());
    }

    eka2l1::ptr<std::uint32_t> bitmap_manipulator::get_fbs_data_with_handle(eka2l1::thread_ptr &call_thr,
        int handle) {
        // Here I'm assumes that the caller of Window Server should have at least touch bitmap if it reached here
        // So the FBS session must be stored in that thread already
        scratch_host->fbs_session = call_thr->get_tls_slot(fbscli->header.entry_point, 0)->pointer;
        process_ptr own_pr = call_thr->owning_process();

        // There is no call to set the handle, so I have to duplicate
        int err = static_cast<int>(do_call(own_pr, duplicate_func_addr, scratch.ptr_address()));

        if (err != 0) {
            LOG_ERROR("Duplicate bitmap error with code {}", err);
            return 0;
        }

        return get_fbs_data(own_pr, scratch);
    }

    void bitmap_manipulator::done_with_fbs_data_from_handle(eka2l1::process_ptr &call_pr) {
        // After we done with this bitmap, we should reset it
        do_call(call_pr, reset_func_addr, scratch.ptr_address());
    }
}