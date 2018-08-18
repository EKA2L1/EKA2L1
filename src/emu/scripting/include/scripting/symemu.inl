#include <scripting/emulog.h>
#include <scripting/hook.h>
#include <scripting/process.h>
#include <scripting/thread.h>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

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
        .def("readProcessMemory", &scripting::process::read_process_memory,
            R"pbdoc(
        Read at the specified address in the process's address space.
        )pbdoc")
        .def("writeProcessMemory", &scripting::process::write_process_memory,
            R"pbdoc(
        Write at the specified address in the process's address space.
        )pbdoc")
        .def("getExecutablePath", &scripting::process::get_executable_path,
            R"pbdoc(
        Get the executable path in Symbian filesystem.
        )pbdoc")
        .def("getName", &scripting::process::get_name,
            R"pbdoc(
        Get the current process name
        )pbdoc")
        .def("getThreadList", &scripting::process::get_thread_list,
                R"pbdoc(
        Get the thread list.
        )pbdoc");

    py::class_<scripting::thread>(m, "Thread")
        .def(py::init([](uint64_t thread_ptr) { return std::make_unique<scripting::thread>(thread_ptr); }))
        .def("getName", &scripting::thread::get_name,
            R"pbdoc(
            Get the name of thread
            )pbdoc")
        .def("getRegister", &scripting::thread::get_register,
            R"pbdoc(
            Get a register of thread context
            )pbdoc")
        .def("getPc", &scripting::thread::get_pc,
            R"pbdoc(
            Get PC of thread context
            )pbdoc")
        .def("getSp", &scripting::thread::get_sp,
            R"pbdoc(
            Get PC of thread context
            )pbdoc")
        .def("getLr", &scripting::thread::get_lr,
            R"pbdoc(
            Get PC of thread context
            )pbdoc")
        .def("getLeaveDepth", &scripting::thread::get_leave_depth,
            R"pbdoc(
            Get the leave depth of thread
            )pbdoc")
        .def("getExitReason", &scripting::thread::get_exit_reason,
            R"pbdoc(
            Get the exit reason of thread
            )pbdoc")
        .def("getOwningProcess", &scripting::thread::get_owning_process,
            R"pbdoc(
            Get the own process of thread.
            )pbdoc")
        .def("getState", &scripting::thread::get_state,
            R"pbdoc(
            Get the own process of thread.
            )pbdoc")
        .def("getPriority", &scripting::thread::get_priority,
            R"pbdoc(
            Get the own process of thread.
            )pbdoc");

    m.def("emulog", &scripting::emulog, R"pbdoc(
        Log to the emulator's logging system
        )pbdoc");

    m.def("registerPanicInvokement", &scripting::register_panic_invokement,
        R"pbdoc(
        Register a function to be called when a panic happen
        )pbdoc");

    m.def("registerSvcInvokement", &scripting::register_svc_invokement,
        R"pbdoc(
        Register a function to be called when a specific SVC is called
        )pbdoc");

    m.def("registerRescheduleInvokement", &scripting::register_reschedule_invokement,
        R"pbdoc(
        Register a function to be called right before a reschedule is started
        )pbdoc");

    m.def("getProcessesList", &scripting::get_process_list,
        R"pbdoc(
        Get a list of processes currently running
        )pbdoc");
}