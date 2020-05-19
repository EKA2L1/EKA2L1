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

#include <common/allocator.h>

static bool faker_next_hook(void *userdata) {
    return reinterpret_cast<eka2l1::service::faker *>(userdata)->walk();
}

namespace eka2l1::service {
    faker::faker(kernel_system *kern, hle::lib_manager *libmngr, const std::string &name, const std::uint32_t uid)
        : process_(nullptr)
        , control_(nullptr)
        , lr_addr_(0)
        , data_offset_(0)
        , initial_(nullptr) {
        initialise(kern, libmngr, name, uid);
    }

    eka2l1::address faker::trampoline_address() const {
        return lr_addr_;
    }

    static constexpr std::uint32_t TEMP_ARGS_REGION = 0x100;

    bool faker::initialise(kernel_system *kern, hle::lib_manager *mngr, const std::string &name, const std::uint32_t uid) {
        // Create the fake process first
        process_ = kern->create<kernel::process>(kern->get_memory_system(), name, u"Nowhere", u"");

        if (!process_) {
            return false;
        }

        // Create control block
        control_ = kern->create<kernel::chunk>(kern->get_memory_system(), process_, name + "_HLEControl",
            0, 0x1000, 0x1000, prot::read_write, kernel::chunk_type::normal, kernel::chunk_access::local,
            kernel::chunk_attrib::none);

        allocator_ = std::make_unique<common::block_allocator>(reinterpret_cast<std::uint8_t *>(
                                                                   control_->host_base())
                + TEMP_ARGS_REGION,
            0x1000 - TEMP_ARGS_REGION);

        if (!control_) {
            return false;
        }

        // Try to load euser.
        codeseg_ptr euser_lib = mngr->load(u"euser.dll", process_);

        if (!euser_lib) {
            return false;
        }

        static const char *TRAMPOLINE_SEG_NAME = "JumpJupTrampoline";

        codeseg_ptr seg = kern->get_by_name<kernel::codeseg>(TRAMPOLINE_SEG_NAME);

        data_offset_ = 200;

        static constexpr std::uint32_t SETUP_THREAD_HEAP_EXPORT_ORD = 1360;
        static constexpr std::uint32_t NEW_TRAP_CLEANUP_EXPORT_ORD = 196;

        if (!seg) {
            kernel::codeseg_create_info create_info;

            create_info.code_base = 0;
            create_info.data_base = 0;
            create_info.data_size = 0;
            create_info.constant_data = nullptr;

            create_info.uids[0] = 0x1000007A;
            create_info.uids[1] = 0;
            create_info.uids[2] = uid;

            // Build trampoline code
            // For each process we are just gonna rebuild the code for now.
            // Not too expensive.
            std::vector<std::uint8_t> trampoline_code;
            trampoline_code.resize(280);

            common::cpu_info context_info;
            context_info.bARMv7 = false;

            common::armgen::armx_emitter emitter(reinterpret_cast<std::uint8_t *>(&trampoline_code[0]), context_info);
            int a = 5;

            // Set up heap by jumping
            // Relocate data:
            // - 0: Control block address.
            // - 4: CreateHeap
            // - 8 : NewTrapCleanup.
            emitter.MOV(common::armgen::R0, common::armgen::R4); // R4 should contains whether the thread is first in process or not.
            emitter.MOV(common::armgen::R1, common::armgen::R_SP); // Stack pointer points to heap thread info.
            emitter.ADD(common::armgen::R11, common::armgen::R_PC, data_offset_ - 16);
            emitter.LDR(common::armgen::R12, common::armgen::R11, 4); // Load export UserHeap::SetupThreadHeap

            emitter.PUSH(1, common::armgen::R11);
            emitter.BL(common::armgen::R12); // Jump to it.

            // Don't have to call InitProcess SVC since we dont have anything actually, this process barely uses stuffs.
            // Try to setup a trap cleanup
            emitter.LDR(common::armgen::R12, common::armgen::R11, 8);
            emitter.BL(common::armgen::R12);
            emitter.POP(1, common::armgen::R11);

            // Load control block address
            emitter.LDR(common::armgen::R11, common::armgen::R11);

            // Let thread semaphore wait. Call WaitForAnyRequest.
            emitter.SVC(0x800000);
            emitter.B(emitter.get_code_pointer() + sizeof(std::uint64_t) * 2);

            // Fiddle some trampoline SVC call that only available here.
            // We are using thumb
            // Reserve:
            // - 1 qword for our callback pointer to do system call.
            // - 1 qword pointing to this class.
            // - The SVC call, which should be ARM. 0xEF0000FF in this case (DFFF - ARM GDB)
            // TODO: This smells hacks. If there is an 128-bit OS in the future than I'm so sorry my lord.
            std::uint64_t *trampoline_ptr = reinterpret_cast<std::uint64_t *>(emitter.get_writeable_code_ptr());

            *trampoline_ptr++ = reinterpret_cast<std::uint64_t>(faker_next_hook);
            *trampoline_ptr++ = reinterpret_cast<std::uint64_t>(this);

            lr_addr_ = static_cast<address>(reinterpret_cast<std::uint8_t *>(trampoline_ptr) - trampoline_code.data());

            *reinterpret_cast<std::uint32_t *>(trampoline_ptr++) = 0xEF0000FF; // SVC #0xFF - HLE host call.

            emitter.set_code_pointer(emitter.get_writeable_code_ptr() + 2 * sizeof(std::uint64_t) + 4);

            // Jump to control LLE code. TODO to preserve R11
            emitter.LDR(common::armgen::R12, common::armgen::R11); // Jump address
            emitter.LDR(common::armgen::R0, common::armgen::R11, 4); // Arg0
            emitter.LDR(common::armgen::R1, common::armgen::R11, 8); // Arg1
            emitter.LDR(common::armgen::R2, common::armgen::R11, 12); // Arg2
            emitter.LDR(common::armgen::R3, common::armgen::R11, 16);
            emitter.PUSH(1, common::armgen::R11); // Preserve control
            emitter.BL(common::armgen::R12);
            emitter.POP(1, common::armgen::R11);

            // Jump to our HLE host dispatch now
            emitter.B(trampoline_code.data() + lr_addr_);

            create_info.code_load_addr = 0;
            create_info.code_data = trampoline_code.data();
            create_info.code_size = static_cast<std::uint32_t>(trampoline_code.size());

            seg = kern->create<kernel::codeseg>(TRAMPOLINE_SEG_NAME, create_info);
        }

        seg->attach(process_);

        std::uint8_t *trampoline = nullptr;

        // Relocate some stuff
        std::uint32_t addr = seg->get_code_run_addr(process_, &trampoline);
        std::uint32_t *trampoline32 = reinterpret_cast<std::uint32_t *>(trampoline + data_offset_);

        // Relocate export.
        trampoline32[0] = control_->base().ptr_address();
        trampoline32[1] = euser_lib->lookup(process_, SETUP_THREAD_HEAP_EXPORT_ORD);
        trampoline32[2] = euser_lib->lookup(process_, NEW_TRAP_CLEANUP_EXPORT_ORD);

        // Initialise the initial chain
        initial_ = new chain(this);

        static constexpr std::uint32_t STACK_SIZE = 0x10000;

        // Initialise our process and run
        process_->construct_with_codeseg(seg, STACK_SIZE, 0x10000, 0xF0000, kernel::process_priority::high);
        E_LOFF(process_->get_thread_list().first(), kernel::thread, process_thread_link)->rename(name + "_HLETHread");
        process_->run();

        // Done now! Thanks everyone.
        return true;
    }

    bool faker::walk() {
        if (!initial_ || initial_->type_ == chain::chain_type::unk) {
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
                initial_->data_.func_(this, initial_->data_.userdata_);
                cycle_next();
            }
        } else {
            for (const auto &to_free : free_lists_) {
                allocator_->free(to_free);
            }

            free_lists_.clear();

            // Write control block
            std::uint32_t *control_block = reinterpret_cast<std::uint32_t *>(control_->host_base());
            control_block[0] = initial_->data_.raw_func_addr_;

            for (std::uint32_t i = 0; i < sizeof(initial_->data_.raw_func_args_) / sizeof(std::uint32_t); i++) {
                control_block[i + 1] = initial_->data_.raw_func_args_[i];
            }

            free_lists_.insert(free_lists_.end(), initial_->free_lists_.begin(), initial_->free_lists_.end());

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

    eka2l1::address faker::new_temporary_argument_with_size(const std::uint32_t size, std::uint8_t **pointer) {
        std::uint8_t *result = reinterpret_cast<std::uint8_t *>(allocator_->allocate(size));

        if (!result) {
            return 0;
        }

        if (pointer) {
            *pointer = result;
        }

        eka2l1::address offset = static_cast<eka2l1::address>(result - reinterpret_cast<std::uint8_t *>(control_->host_base()));

        faker::chain *end = get_end_chain(initial_);

        end->free_lists_.push_back(result);
        return offset + control_->base().ptr_address();
    }

    std::uint8_t *faker::get_last_temporary_argument_impl(const int number) {
        if (free_lists_.size() <= number) {
            return nullptr;
        }

        return free_lists_[number];
    }

    std::uint32_t faker::get_native_return_value() const {
        return E_LOFF(process_->get_thread_list().first(), kernel::thread, process_thread_link)->get_thread_context().cpu_registers[0];
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

    kernel::thread *faker::main_thread() {
        return E_LOFF(process_->get_thread_list().first(), kernel::thread, process_thread_link);
    }

    faker::chain::chain(faker *daddy)
        : daddy_(daddy)
        , next_(nullptr)
        , type_(faker::chain::chain_type::unk) {
    }

    static void notify_native_execution(faker *daddy) {
        faker::chain *first = daddy->initial();

        while (first->next_ != nullptr && first->type_ != faker::chain::chain_type::raw_code) {
            first = first->next_;
        }

        if (!first || first->type_ != faker::chain::chain_type::raw_code) {
            daddy->main_thread()->signal_request();
        }
    }

    faker::chain *faker::chain::then(void *userdata, faker::chain::chain_func func) {
        type_ = faker::chain::chain_type::hook;
        data_.userdata_ = userdata;
        data_.func_ = func;

        next_ = new faker::chain(daddy_);
        return next_;
    }

    faker::chain *faker::chain::then(eka2l1::ptr<void> raw_func_addr) {
        notify_native_execution(daddy_);

        type_ = faker::chain::chain_type::raw_code;
        data_.raw_func_addr_ = raw_func_addr.ptr_address();

        next_ = new faker::chain(daddy_);
        return next_;
    }

    faker::chain *faker::chain::then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1) {
        notify_native_execution(daddy_);

        type_ = faker::chain::chain_type::raw_code;
        data_.raw_func_addr_ = raw_func_addr.ptr_address();
        data_.raw_func_args_[0] = arg1;

        next_ = new faker::chain(daddy_);
        return next_;
    }

    faker::chain *faker::chain::then(eka2l1::ptr<void> raw_func_addr, const std::uint32_t arg1, const std::uint32_t arg2) {
        notify_native_execution(daddy_);

        type_ = faker::chain::chain_type::raw_code;
        data_.raw_func_addr_ = raw_func_addr.ptr_address();
        data_.raw_func_args_[0] = arg1;
        data_.raw_func_args_[1] = arg2;

        next_ = new faker::chain(daddy_);
        return next_;
    }
}