#include <scripting/cpu.h>
#include <scripting/instance.h>

#include <core/arm/jit_interface.h>
#include <core/core.h>

namespace eka2l1::scripting {
    uint32_t cpu::get_register(const int index) {
        return get_current_instance()->get_cpu()->get_reg(index);
    }

    uint32_t cpu::get_cpsr() {
        return get_current_instance()->get_cpu()->get_cpsr();
    }

    uint32_t cpu::get_pc() {
        return get_current_instance()->get_cpu()->get_pc();
    }

    uint32_t cpu::get_sp() {
        return get_current_instance()->get_cpu()->get_sp();
    }

    uint32_t cpu::get_lr() {
        return get_current_instance()->get_cpu()->get_lr();
    }
}