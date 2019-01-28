/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <llvm/ExecutionEngine/Orc/CompileUtils.h>

#include <arm/arm_llvm_recompiler.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/virtualmem.h>

#include <utility>
#include <sstream>

namespace eka2l1::arm {
    llvm::JITSymbol arm_memory_manager::findSymbol(const std::string &name) {
        // Find it in link table. Runtime functions
        // Use a reference here, so we can cache all next symbols
        auto &addr = links[name];

        if (!addr) {
            // We request LLVM to find it
            addr = llvm::RTDyldMemoryManager::getSymbolAddress(name);

            if (!addr) {
                LOG_ERROR("LLVM: Symbol {} not found, linkage failed", name);
                addr = reinterpret_cast<std::uint64_t>(null);
            }
        }

        return { addr, llvm::JITSymbolFlags::Exported };
    }

    std::uint8_t *arm_memory_manager::allocateCodeSection(std::uintptr_t size, std::uint32_t align, std::uint32_t sec_id,
        llvm::StringRef sec_name) {
        const std::lock_guard<std::mutex> guard(lock);

        // Like RPCS3, we simply allocates this
        // TODO: More nice
        std::uintptr_t sec_ptr = eka2l1::common::align(reinterpret_cast<std::uintptr_t>(*next + size), 4096);
        
        if (sec_ptr > reinterpret_cast<std::uintptr_t>(org + size)) {
            LOG_ERROR("LLVM: Out of code section memory");
            return nullptr;
        }

        common::commit(reinterpret_cast<void*>(sec_ptr), size, prot::read_write_exec);
        return std::exchange(*next, reinterpret_cast<std::uint8_t*>(sec_ptr));
    }

    std::uint8_t *arm_memory_manager::allocateDataSection(std::uintptr_t size, std::uint32_t align, std::uint32_t sec_id,
        llvm::StringRef sec_name, bool is_read_only) {
        const std::lock_guard<std::mutex> guard(lock);
        
        std::uintptr_t sec_ptr = eka2l1::common::align(reinterpret_cast<std::uintptr_t>(*next + size), 4096);
        
        if (sec_ptr > reinterpret_cast<std::uintptr_t>(org + size)) {
            LOG_ERROR("LLVM: Out of code section memory");
            return nullptr;
        }

        common::commit(reinterpret_cast<void*>(sec_ptr), size, prot::read_write);
        return std::exchange(*next, reinterpret_cast<std::uint8_t*>(sec_ptr));
    }

    arm_llvm_recompiler_base::arm_llvm_recompiler_base(
        decltype(object_layer)::GetMemoryManagerFunction get_mem_mngr_func,
        llvm::orc::JITTargetMachineBuilder jit_tmb, llvm::DataLayout dl)
        : object_layer(execution_session, get_mem_mngr_func)
        , compile_layer(execution_session, object_layer, llvm::orc::ConcurrentIRCompiler(std::move(jit_tmb)))
        , data_layout(std::move(dl))
        , mangle(execution_session, data_layout)
        , ctx(llvm::make_unique<llvm::LLVMContext>())
    {
    }

    llvm::LLVMContext &arm_llvm_recompiler_base::get_context() {
        return *ctx.getContext();
    }

    bool arm_llvm_recompiler_base::add(std::unique_ptr<llvm::Module> module) {
        llvm::Error err = compile_layer.add(execution_session.getMainJITDylib(), 
            llvm::orc::ThreadSafeModule(std::move(module), ctx));

        if (err) {
            LOG_CRITICAL("LLVM: Adding module failed");

            return false;
        }

        return true;
    }
}
