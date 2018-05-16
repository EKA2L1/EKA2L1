#pragma once

#include <capstone.h>
#include <string>

namespace eka2l1 {
    class disasm {
		using insn_ptr = std::unique_ptr<cs_insn, std::function<void(cs_insn *)>>;
		using csh_ptr = std::unique_ptr<csh, std::function<void(csh *)>>;

		csh cp_handle;
		insn_ptr cp_insn;

	public:

        void init();
        void shutdown();

        std::string disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb);
    };
}
