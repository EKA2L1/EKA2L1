#pragma once

#include <capstone.h>
#include <memory>
#include <ptr.h>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace arm {
        class jit_interface;
        using jitter = std::unique_ptr<jit_interface>;
    }

    template <class T>
    class ptr;

    class memory;

    enum class arm_inst_type {
        arm = 0x00,
        thumb = 0x01,
        data = 0x02
    };

    struct subroutine {
        std::vector<arm_inst_type> insts;
        ptr<uint8_t> end;
    };

    class disasm {
        using insn_ptr = std::unique_ptr<cs_insn, std::function<void(cs_insn *)>>;
        using csh_ptr = std::unique_ptr<csh, std::function<void(csh *)>>;

        csh cp_handle;
        insn_ptr cp_insn;
        memory_system *mem;

        // This jitter is use for detect subroutine.
        arm::jitter jitter;

    public:
        void init(memory_system *smem);
        void shutdown();

        subroutine get_subroutine(ptr<uint8_t> beg);
        std::string disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb);
    };
}
