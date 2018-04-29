#pragma once

#include <string>

namespace eka2l1 {
    namespace disasm {
        void init();
        void shutdown();

        std::string disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb);
    }
}
