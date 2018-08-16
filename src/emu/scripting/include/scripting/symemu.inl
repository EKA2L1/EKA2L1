#include <scripting/emulog.h>
#include <scripting/hook.h>
#include <scripting/process.h>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

PYBIND11_EMBEDDED_MODULE(symemu, m) {
    m.def("emulog", &scripting::emulog, R"pbdoc(
        Log to the emulator's logging system
    )pbdoc");

    py::class_<scripting::process> process(m, "Process");

    process.def(py::init([](uint64_t process_ptr) { return std::make_unique<scripting::process>(process_ptr); }));

    process.def("readProcessMemory", &scripting::process::read_process_memory,
        R"pbdoc(
        Read at the specified address in the process's address space.
        )pbdoc");

    process.def("writeProcessMemory", &scripting::process::write_process_memory,
        R"pbdoc(
        Write at the specified address in the process's address space.
        )pbdoc");

    m.def("registerPanicInvokement", &scripting::register_panic_invokement,
        R"pbdoc(
        Register a function to be called when a panic happen
        )pbdoc");

    m.def("registerSvcInvokement", &scripting::register_svc_invokement,
        R"pbdoc(
        Register a function to be called when a specific SVC is called
        )pbdoc");
}