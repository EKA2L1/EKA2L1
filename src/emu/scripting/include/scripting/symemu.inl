#include <scripting/codeseg.h>
#include <scripting/cpu.h>
#include <scripting/emulog.h>
#include <scripting/hook.h>
#include <scripting/mem.h>
#include <scripting/message.h>
#include <scripting/process.h>
#include <scripting/server.h>
#include <scripting/session.h>
#include <scripting/thread.h>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <kernel/process.h>
#include <kernel/thread.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

PYBIND11_EMBEDDED_MODULE(symemu, m) {
    py::enum_<eka2l1::kernel::thread_state>(m, "ThreadState", R"pbdoc(
            Representing current state of a thread
        )pbdoc")
        .value("RUN", eka2l1::kernel::thread_state::run, R"pbdoc(
            The thread is running.
        )pbdoc")
        .value("STOP", eka2l1::kernel::thread_state::stop, R"pbdoc(
            The thread has been stopped.
        )pbdoc")
        .value("WAIT", eka2l1::kernel::thread_state::wait, R"pbdoc(
            The thread is being suspended.
        )pbdoc")
        .value("WAIT_DFC", eka2l1::kernel::thread_state::wait_dfc, R"pbdoc(
            The thread is waiting for the DFC queue.
        )pbdoc")
        .value("WAIT_FAST_SEMA", eka2l1::kernel::thread_state::wait_fast_sema, R"pbdoc(
            The thread is currently waiting for a semaphore to be freed.
        )pbdoc")
        .value("WAIT_MUTEX", eka2l1::kernel::thread_state::wait_mutex, R"pbdoc(
            The thread is currently waiting for a mutex to be freed.
        )pbdoc")
        .value("WAIT_FAST_SEMA_SUSPENDED", eka2l1::kernel::thread_state::wait_fast_sema_suspend, R"pbdoc(
            The thread has been suspended while waiting for a fast sema to be freed.
        )pbdoc")
        .value("WAIT_MUTEX_SUSPENDED", eka2l1::kernel::thread_state::wait_mutex_suspend, R"pbdoc(
            The thread has been suspended while waiting for a mutex to be freed.
        )pbdoc")
        .value("HOLD_MUTEX_PENDING", eka2l1::kernel::thread_state::hold_mutex_pending, R"pbdoc(
            The thread has been removed from mutex's waiting queue, and is waiting to accquire the mutex.
        )pbdoc")
        .value("READY", eka2l1::kernel::thread_state::ready, R"pbdoc(
            The thread has been queued to the scheduler and is ready to be ran.
        )pbdoc");

    py::enum_<eka2l1::kernel::thread_priority>(m, "ThreadPriority", R"pbdoc(
            Representing the importance of a thread to be scheduled for running"
        )pbdoc")
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

            The function can throw an exception in case the memory at the given address in the memory
            space is not yet mapped, or the size of the data exceeds the mapped memory size from that
            address fowards.

            Parameters
            ----------
            addr: int
                  The address that we want to read from.

            size: int
                  Size of data we want to read.

            Returns
            -------
            str
                A string contains the data.
        )pbdoc")
        .def("writeProcessMemory", &scripting::process::write_process_memory, R"pbdoc(
            Write at the specified address in the process's address space.
            
            The function can throw an exception in case the memory at the given address in the memory
            space is not yet mapped, or the size of the data exceeds the mapped memory size from that
            address fowards.

            Parameters
            ----------
            addr: int
                  The address that we want to read from.

            dat: str
                  The data to write to that memory address in the process's address space.
        )pbdoc")
        .def("readByte", &scripting::process::read_byte, R"pbdoc(
            "Read a byte from process memory at given address.
        )pbdoc")
        .def("readWord", &scripting::process::read_word, R"pbdoc(
            "Read a word from process memory at given address.
        )pbdoc")
        .def("readDword", &scripting::process::read_dword, R"pbdoc(
            "Read a double word from process memory at given address.
        )pbdoc")
        .def("readQword", &scripting::process::read_qword, R"pbdoc(
            "Read a quad word from process memory at given address.
        )pbdoc")
        .def("getExecutablePath", &scripting::process::get_executable_path, R"pbdoc(
            Get the executable path in Symbian filesystem.
        )pbdoc")
        .def("getName", &scripting::process::get_name, R"pbdoc(
            Get the process's name
        )pbdoc")
        .def("getThreadList", &scripting::process::get_thread_list, R"pbdoc(
            Get all the thread that the process owns.

            Returns
            -------
            List[symemu.Thread]
                A list of threads that this process owns.
        )pbdoc");

    py::class_<scripting::thread>(m, "Thread")
        .def(py::init([](uint64_t thread_ptr) { return std::make_unique<scripting::thread>(thread_ptr); }))
        .def("getName", &scripting::thread::get_name, R"pbdoc(
            Get the name of thread.
        )pbdoc")
        .def("getRegister", &scripting::thread::get_register, R"pbdoc(
            Get a register of thread context.

            Parameters
            ----------
            idx: int
                The index of the register, from 0 to 15.

            Returns
            -------
            int
                The value of the register.
        )pbdoc")
        .def("getPc", &scripting::thread::get_pc, R"pbdoc(
            Get PC of thread context.
        )pbdoc")
        .def("getSp", &scripting::thread::get_sp, R"pbdoc(
            Get PC of thread context.
        )pbdoc")
        .def("getLr", &scripting::thread::get_lr, R"pbdoc(
            Get PC of thread context.
        )pbdoc")
        .def("getLeaveDepth", &scripting::thread::get_leave_depth, R"pbdoc(
            Get the leave depth of thread.
        )pbdoc")
        .def("getExitReason", &scripting::thread::get_exit_reason, R"pbdoc(
            Get the exit reason of thread
        )pbdoc")
        .def("getOwningProcess", &scripting::thread::get_owning_process, R"pbdoc(
            Get the own process of thread.
        )pbdoc")
        .def("getState", &scripting::thread::get_state, R"pbdoc(
            Get the current state of the thread. States are defined in ThreadState enum.
        )pbdoc")
        .def("getPriority", &scripting::thread::get_priority, R"pbdoc(
            Get the own process of thread.
        )pbdoc")
        .def("getStackBase", &scripting::thread::get_stack_base, R"pbdoc(
            Get the base address of the stack memory chunk.
        )pbdoc")
        .def("getHeapBase", &scripting::thread::get_heap_base, R"pbdoc(
            Get the base address of the heap memory chunk.
        )pbdoc");

    py::class_<scripting::codeseg>(m, "Codeseg")
        .def(py::init([](uint64_t handle) { return std::make_unique<scripting::codeseg>(handle); }))
        .def("lookup", &scripting::codeseg::lookup, R"pbdoc(
            Lookup address of a symbol with given ordinal.

            Parameters
            ----------
            ord: int
                The ordinal of the export function to get.

            Returns
            -------
            int
                The address of the export function.
        )pbdoc")
        .def("codeRunAddress", &scripting::codeseg::code_run_address, R"pbdoc(
            Get runtime code section address.
        )pbdoc")
        .def("dataRunAddress", &scripting::codeseg::data_run_address, R"pbdoc(
            Get runtime data section address.
        )pbdoc")
        .def("bssRunAddress", &scripting::codeseg::bss_run_address, R"pbdoc(
            Get runtime BSS address.
        )pbdoc")
        .def("codeSize", &scripting::codeseg::code_size, R"pbdoc(
            Get code section size.
        )pbdoc")
        .def("dataSize", &scripting::codeseg::data_size, R"pbdoc(
            Get data section size.
        )pbdoc")
        .def("bssSize", &scripting::codeseg::bss_size, R"pbdoc(
            Get bss section size.
        )pbdoc")
        .def("exportCount", &scripting::codeseg::get_export_count, R"pbdoc(
            Get total exports provided by this code segment.
        )pbdoc");

    py::class_<scripting::ipc_message_wrapper>(m, "IpcMessage")
        .def(py::init([](const std::uint64_t handle) { return std::make_unique<scripting::ipc_message_wrapper>(handle); }))
        .def("function", &scripting::ipc_message_wrapper::function, R"pbdoc(
            Get opcode ordinal of this message.
        )pbdoc")
        .def("sender", &scripting::ipc_message_wrapper::sender, R"pbdoc(
            Get the thread that sends and owns the message.
        )pbdoc")
        .def("session", &scripting::ipc_message_wrapper::session, R"pbdoc(
            Get the session of this message.
        )pbdoc")
        .def("arg", &scripting::ipc_message_wrapper::arg, R"pbdoc(
            Get the argument at specified index.

            Parameters
            ------------------------
            idx: int
                 The index of the IPC argument. Must be in [0, 3].

            Returns
            ------------------------
            int
                 An integer, which is the value of argument at the given index.
        )pbdoc")
        .def("flags", &scripting::ipc_message_wrapper::flags, R"pbdoc(
            Get IPC message flags, containing information about argument types.
        )pbdoc");

    py::class_<scripting::session_wrapper>(m, "Session")
        .def(py::init([](std::uint64_t handle) { return std::make_unique<scripting::session_wrapper>(handle); }))
        .def("server", &scripting::session_wrapper::server, R"pbdoc(
            Get the target server of this session.
        )pbdoc");

    py::class_<scripting::server_wrapper>(m, "Server")
        .def(py::init([](std::uint64_t handle) { return std::make_unique<scripting::server_wrapper>(handle); }))
        .def("getName", &scripting::server_wrapper::get_name, R"pbdoc(
            Get name of the server.
        )pbdoc");

    py::class_<scripting::cpu>(m, "Cpu")
        .def_static("getReg", &scripting::cpu::get_register, R"pbdoc(
            Get a register of the CPU.
        )pbdoc")
        .def_static("setReg", &scripting::cpu::set_register, R"pbdoc(
            Set a register of the CPU.
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

    m.def("registerLibraryInvokement", &scripting::register_lib_invokement, R"pbdoc(
        Register a function to be called when a library function is called.
        
        Parameters
        -----------------
        ord: int
                     Ordinal of the function we want to hook with.

        process_uid: int
                     UID of the process we want to hook with.

        func
                     The function to invoke on breakpoint hit.
    )pbdoc");

    m.def("registerBreakpointInvokement", &scripting::register_breakpoint_invokement, R"pbdoc(
        Register a function to be called when a breakpoint is hit.

        Parameters
        -----------------
        image_name: str
                     Name of the image we want this breakpoint to based on.

                     Use 0 to make the passed address absolute.
                     Use 'PROCESS' to make the passed address based on the current process.

        addr: int
                     Address of the breakpoint. An odd address indicates the code at breakpoint is Thumb.
                     Use the note above to place a thumb breakpoint.

                     It's required to use the ROM address or an address with the base as same as image.

        process_uid: int
                     The UID of the process we want to attach this breakpoint to.

                     Use 0 to make this invoked with every processes.

        func
                     The function to invoke on breakpoint hit.
    )pbdoc");

    m.def("registerRescheduleInvokement", &scripting::register_reschedule_invokement, R"pbdoc(
        Register a function to be called right before a reschedule is started
    )pbdoc");

    m.def("registerIpcInvokement", &scripting::register_ipc_invokement, R"pbdoc(
        Register a function to be called around IPC event.
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

    m.def("loadCodeseg", &scripting::load_codeseg, R"pbdoc(
        Load a codeseg from an E32Image/ROM image.
    )pbdoc");

    m.def("messageFromHandle", &scripting::message_from_handle, R"pbdoc(
        Retrieve an IPC message object from a handle.

        Parameters
        -----------------
        guestHandle: int
                     A guest handle to the IPC message object.

        Returns
        -----------------
        IpcMessage
                     IPC Message Object.
    )pbdoc");

    m.def("sessionFromHandle", &scripting::session_from_handle, R"pbdoc(
        Retrieve a Session message object from a handle.

        Parameters
        ----------
        guestHandle: int
                     A guest handle to the session.

        Returns
        -------
        Session
                     Session Object.
    )pbdoc");
}
