#pragma once

#include <pybind11/pybind11.h>
#include <string>

namespace eka2l1::scripting {
    void emulog(const std::string &format, pybind11::args vargs);
}