#pragma once

#include <scripting/platform.h>

#if ENABLE_PYTHON_SCRIPTING
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#endif

#include <functional>

namespace eka2l1::scripting {
#if ENABLE_PYTHON_SCRIPTING
    void register_lib_invokement(const std::string &lib_name, const std::uint32_t ord, const std::uint32_t process_uid, pybind11::function func);
    void register_breakpoint_invokement(const std::string &image_name, const uint32_t addr, const std::uint32_t process_uid, pybind11::function func);
    void register_ipc_invokement(const std::string &server_name, const int opcode, const int when, pybind11::function func);
#endif
}
