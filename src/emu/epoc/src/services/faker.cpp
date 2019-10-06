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

#include <common/armemitter.h>

#include <epoc/kernel.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/kernel/process.h>
#include <epoc/services/faker.h>

static bool faker_next_hook(void *userdata) {
    return reinterpret_cast<eka2l1::service::faker*>(userdata)->walk();
}

namespace eka2l1::service {
    faker::faker(kernel_system *kern, hle::lib_manager *libmngr, const std::string &name)
        : process_(nullptr)
        , trampoline_(nullptr)
        , initial_(nullptr) {
        initialise(kern, libmngr, name);
    }

    eka2l1::address faker::trampoline_address() const {
        return lr_addr_;
    }
    
    bool faker::initialise(kernel_system *kern, hle::lib_manager *mngr, const std::string &name) {
        // Create the fake process first
        process_ = kern->create<kernel::process>(kern->get_memory_system(), name, u"Nowhere", u"").get();

        if (!process_) {
            return false;
        }

        // Create the trampoline allowing us to hook HLE callbacks.
        const std::string trampoline_chunk_name = name + "_trampoline";
        trampoline_ = kern->create<kernel::chunk>(kern->get_memory_system(), process_, trampoline_chunk_name, 0,
            1000, 1000, prot::read_write, kernel::chunk_type::normal, kernel::chunk_access::code, kernel::chunk_attrib::none);

        if (!trampoline_) {
            return false;
        }

        // Try to load euser.
        codeseg_ptr euser_lib = mngr->load(u"euser.dll", process_);

        if (!euser_lib) {
            return false;
        }

        static constexpr std::uint32_t SETUP_THREAD_HEAP_EXPORT_ORD = 1360;
        static constexpr std::uint32_t NEW_TRAP_CLEANUP_EXPORT_ORD = 196;

        common::cpu_info context_info;
        context_info.bARMv7 = false;

        common::armgen::armx_emitter emitter(reinterpret_cast<std::uint8_t*>(trampoline_->host_base()), context_info);
        
        data_offset_ = 200;

        // Set up heap by jumping
        emitter.MOV(common::armgen::R0, common::armgen::R4);        // R4 should contains whether the thread is first in process or not.
        emitter.MOV(common::armgen::R1, common::armgen::R_SP);      // Stack pointer points to heap thread info.
        emitter.MOVI2R(common::armgen::R11, trampoline_->base().ptr_address() + data_offset_);
        emitter.MOVI2R(common::armgen::R12, euser_lib->lookup(process_, SETUP_THREAD_HEAP_EXPORT_ORD));     // Load export UserHeap::SetupThreadHeap
        emitter.BL(common::armgen::R12);        // Jump to it.

        // Don't have to call InitProcess SVC since we dont have anything actually, this process barely uses stuffs.
        // Try to setup a trap cleanup
        emitter.MOVI2R(common::armgen::R12, euser_lib->lookup(process_, NEW_TRAP_CLEANUP_EXPORT_ORD));
        emitter.BL(common::armgen::R12);      

        // Let thread semaphore wait. Call WaitForAnyRequest.
        emitter.SVC(0x800000);
        emitter.B(reinterpret_cast<std::uint8_t*>(trampoline_->host_base()) + sizeof(std::uint64_t) * 2);

        // Fiddle some trampoline SVC call that only available here.
        // We are using thumb
        // Reserve:
        // - 1 qword for our callback pointer to do system call.
        // - 1 qword pointing to this class.
        // - The SVC call, which should be ARM. 0xEF0000FF in this case (DFFF - ARM GDB)
        // TODO: This smells hacks. If there is an 128-bit OS in the future than I'm so sorry my lord.
        std::uint64_t *trampoline_ptr = reinterpret_cast<std::uint64_t*>(emitter.get_writeable_code_ptr());

        *trampoline_ptr++ = reinterpret_cast<std::uint64_t>(faker_next_hook);
        *trampoline_ptr++ = reinterpret_cast<std::uint64_t>(this);
        
        lr_addr_ = static_cast<address>(reinterpret_cast<std::uint8_t*>(trampoline_ptr) - 
            reinterpret_cast<std::uint8_t*>(trampoline_->host_base()));

        *reinterpret_cast<std::uint32_t*>(trampoline_ptr++) = 0xEF0000FF; // SVC #0xFF - HLE host call.

        // Jump to control LLE code. TODO to preserve R11
        emitter.LDR(common::armgen::R12, common::armgen::R11);  // Jump address
        emitter.LDR(common::armgen::R0, common::armgen::R11, 4);    // Arg0
        emitter.LDR(common::armgen::R1, common::armgen::R11, 8);    // Arg1
        emitter.LDR(common::armgen::R2, common::armgen::R11, 12);   // Arg2
        emitter.LDR(common::armgen::R3, common::armgen::R11, 16);
        emitter.BL(common::armgen::R12);

        // Jump to our HLE host dispatch now
        emitter.B(reinterpret_cast<std::uint8_t*>(trampoline_->host_base()) + lr_addr_);

        // Intialise the initial chain
        initial_ = new chain(this);

        // Done now! Thanks everyone.
        return true;
    }

    bool faker::walk() {
        if (initial_) {
            return false;
        }

        auto cycle_next = [&]() {    
            chain *org = initial_;
            initial_ = initial_->next_;

            delete org;
        };

        if (initial_->type_ == chain::chain_type::hook) {
            // Run the callback while it's still hook
            while (initial_ && initial_->type_ == chain::chain_type::hook) {
                // Run the hook
                initial_->data_.func_(initial_->data_.userdata_);
                cycle_next();
            }
        } else {
            // Write control block
            std::uint8_t *control_block = reinterpret_cast<std::uint8_t*>(trampoline_->host_base()) + data_offset_;
            control_block[0] = initial_->data_.raw_func_addr_;

            for (std::uint32_t i = 0; i < sizeof(initial_->data_.raw_func_args_) / sizeof(std::uint32_t); i++) {
                control_block[i + 1] = initial_->data_.raw_func_args_[i];
            }

            // Run it native
            cycle_next();
        }

        return true;
    }

    static faker::chain *get_end_chain(faker::chain *beginning) {
        faker::chain *my_end = beginning;

        while (my_end->next_ != nullptr) {
            my_end = my_end->next_;
        }

        return my_end;
    }

    faker::chain *faker::then(void *userdata, faker::chain::chain_func func) {
        return get_end_chain(initial_)->then(userdata, func);
    }
        
    faker::chain *faker::then(eka2l1::ptr<void> raw_func_addr) {
        return get_end_chain(initial_)->then(raw_func_addr);
    }

    faker::chain *faker::then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1) {
        return get_end_chain(initial_)->then(raw_func_addr, arg1);
    }

    faker::chain *faker::then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1, const std::uint32_t arg2) {
        return get_end_chain(initial_)->then(raw_func_addr, arg2);
    }

    faker::chain::chain(faker *daddy) 
        : daddy_(daddy)
        , next_(nullptr) {

    }

    faker::chain *faker::chain::then(void *userdata, faker::chain::chain_func func) {
        type_ = faker::chain::chain_type::hook;
        data_.userdata_ = userdata;
        data_.func_ = func;

        next_ = new faker::chain(daddy_);
        return next_;
    }

    faker::chain *faker::chain::then(eka2l1::ptr<void> raw_func_addr) {
        type_ = faker::chain::chain_type::raw_code;
        data_.raw_func_addr_ = raw_func_addr.ptr_address();

        next_ = new faker::chain(daddy_);
        return next_;
    }

    faker::chain *faker::chain::then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1) {
        type_ = faker::chain::chain_type::raw_code;
        data_.raw_func_addr_ = raw_func_addr.ptr_address();
        data_.raw_func_args_[0] = arg1;

        next_ = new faker::chain(daddy_);
        return next_;
    }

    faker::chain *faker::chain::then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1, const std::uint32_t arg2) {
        type_ = faker::chain::chain_type::raw_code;
        data_.raw_func_addr_ = raw_func_addr.ptr_address();
        data_.raw_func_args_[0] = arg1;
        data_.raw_func_args_[1] = arg2;

        next_ = new faker::chain(daddy_);
        return next_;
    }
}