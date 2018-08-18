#include <pybind11/embed.h>

#include <scripting/thread.h>
#include <scripting/process.h>

namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    thread::thread(uint64_t handle)
        : thread_handle(std::move(*reinterpret_cast<eka2l1::thread_ptr *>(handle))) {
    }

    std::string thread::get_name() {
        return thread_handle->name();
    }

    uint32_t thread::get_register(uint8_t index) {
        if (thread_handle->get_thread_context().cpu_registers.size() <= index) {
            throw pybind11::index_error("CPU Register Index is out of range");
        }

        return thread_handle->get_thread_context().cpu_registers[index];
    }

    uint32_t thread::get_pc() {
        return thread_handle->get_thread_context().pc;
    }

    uint32_t thread::get_lr() {
        return thread_handle->get_thread_context().lr;
    }

    uint32_t thread::get_sp() {
        return thread_handle->get_thread_context().sp;
    }

    uint32_t thread::get_cpsr() {
        return thread_handle->get_thread_context().cpsr;
    }

    int thread::get_exit_reason() {
        return thread_handle->get_exit_reason();
    }

    int thread::get_leave_depth() {
        return thread_handle->get_leave_depth();
    }

    kernel::thread_state thread::get_state() {
        return thread_handle->current_state();
    }

    kernel::thread_priority thread::get_priority() {
        return thread_handle->get_priority();
    }

    std::unique_ptr<scripting::process> thread::get_owning_process() {
        return std::make_unique<scripting::process>((uint64_t)(&thread_handle->owning_process()));
    }
}