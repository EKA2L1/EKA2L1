#pragma once

#include <core/kernel/thread.h>
#include <memory>

namespace eka2l1::scripting {
    class process;

    class thread {
        eka2l1::thread_ptr thread_handle;

        friend class eka2l1::kernel::thread;
    public:
        thread(uint64_t handle);

        std::string get_name();

        uint32_t get_register(uint8_t index);

        uint32_t get_pc();
        uint32_t get_lr();
        uint32_t get_sp();
        uint32_t get_cpsr();

        int get_exit_reason();
        int get_leave_depth();

        eka2l1::kernel::thread_state get_state();
        eka2l1::kernel::thread_priority get_priority();

        std::unique_ptr<scripting::process> get_owning_process();
    };
}