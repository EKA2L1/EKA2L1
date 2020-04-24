#pragma once

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <functional>

namespace eka2l1::scripting {
    void register_panic_invokement(const std::string &category, pybind11::function func);
    void register_svc_invokement(int svc_num, int time, pybind11::function func);
    void register_reschedule_invokement(pybind11::function func);
    void register_lib_invokement(const std::string &lib_name, const std::uint32_t ord, const std::uint32_t process_uid, pybind11::function func);
    void register_breakpoint_invokement(const std::string &image_name, const uint32_t addr, const std::uint32_t process_uid, pybind11::function func);
    void register_ipc_invokement(const std::string &server_name, const int opcode, const int when, pybind11::function func);
}
