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

#pragma once

#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>

#include <arm/arm_analyser.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

namespace eka2l1::arm {
    class arm_memory_manager: public llvm::RTDyldMemoryManager {
        std::unordered_map<std::string, std::uint64_t> links;
        std::uint8_t **next;

        std::uint8_t *org;
        std::size_t   size;

        std::mutex lock;

        // A method for unkown symbol. Throws exception
        [[noreturn]] static void null() {
            throw std::runtime_error("NULL function called");
        }

        explicit arm_memory_manager(std::uint8_t *org, std::size_t size, std::uint8_t **next) 
            : org(org), size(size), next(next) {
        }

        // This will responsible for finding symbol
        // There are some symbol that is external, which will be linked in another module
        // For those that is at runtime, we have a link table
        llvm::JITSymbol findSymbol(const std::string &name) override;

        std::uint8_t *allocateCodeSection(std::uintptr_t size, std::uint32_t align, std::uint32_t sec_id,
            llvm::StringRef sec_name);

        std::uint8_t *allocateDataSection(std::uintptr_t size, std::uint32_t align, std::uint32_t sec_id,
            llvm::StringRef sec_name, bool is_read_only);
    };

    class arm_llvm_recompiler_base {
    protected:
        llvm::orc::ExecutionSession execution_session;
        llvm::orc::RTDyldObjectLinkingLayer object_layer;
        llvm::orc::IRCompileLayer compile_layer;
        llvm::orc::MangleAndInterner mangle;
        llvm::orc::ThreadSafeContext ctx;
        
        llvm::DataLayout data_layout;

    public: 
        explicit arm_llvm_recompiler_base(
            decltype(object_layer)::GetMemoryManagerFunction get_mem_mngr_func,
            llvm::orc::JITTargetMachineBuilder jit_tmb, 
            llvm::DataLayout dl);

        llvm::LLVMContext &get_context();
        virtual bool add(std::unique_ptr<llvm::Module> module);
    };

    using arm_inst_ptr = std::shared_ptr<arm::arm_instruction_base>;

    class arm_llvm_inst_recompiler: public arm_llvm_recompiler_base {
        llvm::IRBuilder<> *builder;
        llvm::Module      *module;

        llvm::Function    *function;
        llvm::StructType  *cpu_context_type;

        llvm::Value       *page_table;
        llvm::Type        *page_table_type;

    public:
        // Get memory as uint8_t*/uint16_t*/... Type is an integer
        llvm::Value *get_mem(llvm::Value *addr, llvm::Type *type);

        explicit arm_llvm_inst_recompiler(llvm::Module *module,
            decltype(object_layer)::GetMemoryManagerFunction get_mem_mngr_func,
            llvm::orc::JITTargetMachineBuilder jit_tmb, 
            llvm::DataLayout dl);

        void translate();
        void ADD(arm_inst_ptr inst);
    };
}