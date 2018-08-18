#pragma once

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <functional>

namespace eka2l1::scripting {
    void register_panic_invokement(const std::string &category, pybind11::function func);
    void register_svc_invokement(int svc_num, pybind11::function func);
    void register_reschedule_invokement(pybind11::function func);
}