#include <scripting/cpu.h>
#include <scripting/emulog.h>
#include <scripting/hook.h>
#include <scripting/mem.h>
#include <scripting/process.h>
#include <scripting/thread.h>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <epoc/kernel/thread.h>
#include <epoc/kernel/process.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

PYBIND11_EMBEDDED_MODULE(symemu, m) {
    py::enum_<eka2l1::kernel::thread_state>(m, "ThreadState")
        .value("RUN", eka2l1::kernel::thread_state::run)
        .value("STOP", eka2l1::kernel::thread_state::stop)
        .value("WAIT", eka2l1::kernel::thread_state::wait)
        .value("WAIT_DFC", eka2l1::kernel::thread_state::wait_dfc)
        .value("WAIT_FAST_SEMA", eka2l1::kernel::thread_state::wait_fast_sema)
        .value("READY", eka2l1::kernel::thread_state::ready);

    py::enum_<eka2l1::kernel::thread_priority>(m, "ThreadPriority")
        .value("NULL", eka2l1::kernel::priority_null)
        .value("MUCH_LESS", eka2l1::kernel::priority_much_less)
        .value("LESS", eka2l1::kernel::priority_much_less)
        .value("NORMAL", eka2l1::kernel::priority_normal)
        .value("MORE", eka2l1::kernel::priority_more)
        .value("MUCH_MORE", eka2l1::kernel::priority_much_more)
        .value("REAL_TIME", eka2l1::kernel::priority_real_time)
        .value("ABSOLUTE_VERY_LOW", eka2l1::kernel::priority_absolute_very_low)
        .value("ABSOLUTE_LOW", eka2l1::kernel::priority_absolute_low)
        .value("ABSOLUTE_BACKGROUND", eka2l1::kernel::priority_absolute_background)
        .value("ABSOLUTE_HIGH", eka2l1::kernel::priority_absolute_high)
        .value("ABSOLUTE_FOREGROUND", eka2l1::kernel::priorty_absolute_foreground);

    py::class_<scripting::process>(m, "Process")
        .def(py::init([](uint64_t process_ptr) { return std::make_unique<scripting::process>(process_ptr); }))
        .def("readProcessMemory", &scripting::process::read_process_memory, R"pbdoc(
            Read at the specified address in the process's address space.
        )pbdoc")
        .def("writeProcessMemory", &scripting::process::write_process_memory, R"pbdoc(
            Write at the specified address in the process's address space.
        )pbdoc")
        .def("getExecutablePath", &scripting::process::get_executable_path, R"pbdoc(
            Get the executable path in Symbian filesystem.
        )pbdoc")
        .def("getName", &scripting::process::get_name, R"pbdoc(
            Get the process's name
        )pbdoc")
        .def("getThreadList", &scripting::process::get_thread_list, R"pbdoc(
            Get the thread list.
        )pbdoc");

    py::class_<scripting::thread>(m, "Thread")
        .def(py::init([](uint64_t thread_ptr) { return std::make_unique<scripting::thread>(thread_ptr); }))
        .def("getName", &scripting::thread::get_name,R"pbdoc(
            Get the name of thread
        )pbdoc")
        .def("getRegister", &scripting::thread::get_register, R"pbdoc(
            Get a register of thread context
        )pbdoc")
        .def("getPc", &scripting::thread::get_pc, R"pbdoc(
            Get PC of thread context
        )pbdoc")
        .def("getSp", &scripting::thread::get_sp, R"pbdoc(
            Get PC of thread context
        )pbdoc")
        .def("getLr", &scripting::thread::get_lr, R"pbdoc(
            Get PC of thread context
        )pbdoc")
        .def("getLeaveDepth", &scripting::thread::get_leave_depth, R"pbdoc(
            Get the leave depth of thread
        )pbdoc")
        .def("getExitReason", &scripting::thread::get_exit_reason, R"pbdoc(
            Get the exit reason of thread
        )pbdoc")
        .def("getOwningProcess", &scripting::thread::get_owning_process, R"pbdoc(
            Get the own process of thread.
        )pbdoc")
        .def("getState", &scripting::thread::get_state, R"pbdoc(
            Get the current state of the thread. 
            
            States are defined in ThreadState enum.
        )pbdoc")
        .def("getPriority", &scripting::thread::get_priority,
        R"pbdoc(
            Get the own process of thread.
        )pbdoc");

    py::class_<scripting::cpu>(m, "Cpu")
        .def_static("getReg", &scripting::cpu::get_register, R"pbdoc(
            Get a register of the CPU.
        )pbdoc")
        .def_static("getCpsr", &scripting::cpu::get_pc, R"pbdoc(
            Get CPU's cpsr.
        )pbdoc")
        .def_static("getLr", &scripting::cpu::get_lr, R"pbdoc(
            Get CPU's lr.
        )pbdoc")
        .def_static("getSp", &scripting::cpu::get_sp, R"pbdoc(
            Get CPU's sp.
        )pbdoc");

    m.def("emulog", &scripting::emulog, R"pbdoc(
        Log to the emulator's logging system
    )pbdoc");

    m.def("registerPanicInvokement", &scripting::register_panic_invokement, R"pbdoc(
        Register a function to be called when a panic happen
    )pbdoc");

    m.def("registerSvcInvokement", &scripting::register_svc_invokement, R"pbdoc(
        Register a function to be called when a specific SVC is called
    )pbdoc");

    m.def("registerLibraryInvokement", &scripting::register_lib_invokement, R"pbdoc(
        Register a function to be called when a library function is called
    )pbdoc");

    m.def("registerBreakpointInvokement", &scripting::register_breakpoint_invokement, R"pbdoc(
        Register a function to be called when a breakpoint is hit.
        
        Only works on fallback (Dynarmic) and Unicorn JIT.
    )pbdoc");

    m.def("registerRescheduleInvokement", &scripting::register_reschedule_invokement, R"pbdoc(
        Register a function to be called right before a reschedule is started
    )pbdoc");

    m.def("getProcessesList", &scripting::get_process_list, R"pbdoc(
        Get a list of processes currently running
    )pbdoc");

    m.def("readByte", &scripting::read_byte, R"pbdoc(
        Read a byte from current process address space.
    )pbdoc");

    m.def("readDword", &scripting::read_dword, R"pbdoc(
        Read a dword from current process address space.
    )pbdoc");

    m.def("readWord", &scripting::read_word, R"pbdoc(
        Read a word from current process address space.
    )pbdoc");

    m.def("readQword", &scripting::read_qword, R"pbdoc(
        Read a qword from current process address space.
    )pbdoc");

    m.def("getCurrentProcess", &scripting::get_current_process, R"pbdoc(
        Get kernel's current process.
    )pbdoc");
        
    m.def("getCurrentThread", &scripting::get_current_thread, R"pbdoc(
        Get kernel's current thread.
    )pbdoc");
}
