#include <common/log.h>
#include <common/path.h>

#include <core/manager/script_manager.h>
#include <experimental/filesystem>
#include <pybind11/pybind11.h>

#include <scripting/instance.h>
#include <scripting/symemu.inl>

namespace py = pybind11;
namespace fs = std::experimental::filesystem;

namespace eka2l1::manager {
    script_manager::script_manager(system *sys)
        : sys(sys)
        , interpreter() {
        scripting::set_current_instance(sys);
    }

    bool script_manager::import_module(const std::string &path) {
        const std::string name = eka2l1::filename(path);

        if (modules.find(name) == modules.end()) {
            const auto &crr_path = fs::current_path();
            const auto &pr_path = fs::absolute(fs::path(path).parent_path());

            std::lock_guard<std::mutex> guard(smutex);

            std::error_code sec;
            fs::current_path(pr_path, sec);

            try {
                modules.emplace(name.c_str(), py::module::import(name.data()));
            } catch (py::error_already_set &exec) {
                const char *description = exec.what();

                LOG_WARN("Script compile error: {}", description);
                fs::current_path(crr_path);

                return false;
            }

            fs::current_path(crr_path);

            if (!call_module_entry(name.c_str())) {
                // If the module entry failed, we still success, but not execute any futher method
                return true;
            }
        }

        return true;
    }

    bool script_manager::call_module_entry(const std::string &module) {
        if (modules.find(module) == modules.end()) {
            return false;
        }

        try {
            modules[module].attr("scriptEntry")(reinterpret_cast<uint64_t>(sys));
        } catch (py::error_already_set &exec) {
            return false;
        }

        return true;
    }

    void script_manager::call_panics(const std::string &panic_cage, int err_code) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &panic_function : panic_functions) {
            if (panic_function.first == panic_cage) {
                try {
                    panic_function.second(err_code);
                } catch (py::error_already_set &exec) {
                    LOG_WARN("Script interpreted error: {}", exec.what());
                }
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    void script_manager::call_svcs(int svc_num) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &svc_function : svc_functions) {
            if (svc_function.first == svc_num) {
                try {
                    svc_function.second();
                } catch (py::error_already_set &exec) {
                    LOG_WARN("Script interpreted error: {}", exec.what());
                }
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    void script_manager::register_panic(const std::string &panic_cage, pybind11::function &func) {
        panic_functions.push_back(panic_func(panic_cage, func));
    }

    void script_manager::register_svc(int svc_num, pybind11::function &func) {
        svc_functions.push_back(svc_func(svc_num, func));
    }
}