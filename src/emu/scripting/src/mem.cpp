#include <scripting/instance.h>
#include <scripting/mem.h>

#include <core/core.h>
#include <core/core_mem.h>
#include <core/epoc/des.h>

namespace eka2l1::scripting {
    char read_byte(const uint32_t addr) {
        char result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 1);

        return result;
    }

    short read_short(const uint32_t addr) {
        short result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 2);

        return result;
    }

    uint32_t read_word(const uint32_t addr) {
        uint32_t result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 4);

        return result;
    }

    uint64_t read_qword(const uint32_t addr) {
        uint64_t result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 8);

        return result;
    }
}