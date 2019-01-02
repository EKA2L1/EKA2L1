#pragma once

#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
}

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

        int get_state();
        int get_priority();

        std::unique_ptr<scripting::process> get_owning_process();
    };
}