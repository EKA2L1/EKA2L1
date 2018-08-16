#include <scripting/hook.h>
#include <scripting/instance.h>

#include <core/core.h>

namespace eka2l1::scripting {
    void register_panic_invokement(const std::string &category, pybind11::function func) {
        eka2l1::scripting::get_current_instance()->get_manager_system()->get_script_manager()->register_panic(category, func);
    }

    void register_svc_invokement(int svc_num, pybind11::function func) {
        eka2l1::scripting::get_current_instance()->get_manager_system()->get_script_manager()->register_svc(svc_num, func);
    }
}