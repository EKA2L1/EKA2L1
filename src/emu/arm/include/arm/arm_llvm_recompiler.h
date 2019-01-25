#pragma once

#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>

namespace eka2l1::arm {
    class arm_llvm_recompiler_base {
        llvm::orc::ExecutionSession execution_session;
        llvm::orc::RTDyldObjectLinkingLayer object_layer;
        llvm::orc::IRCompileLayer compile_layer;
        llvm::orc::MangleAndInterner mangle;
        llvm::orc::ThreadSafeContext ctx;
        
        llvm::DataLayout data_layout;
    };
}