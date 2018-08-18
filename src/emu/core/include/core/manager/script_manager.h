#pragma once

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <mutex>
#include <tuple>
#include <unordered_map>

namespace eka2l1 {
    class system;
}

namespace eka2l1::manager {
    using panic_func = std::pair<std::string, pybind11::function>;
    using svc_func = std::pair<int, pybind11::function>;

    /*! \brief A manager for all custom Python scripts of EKA2L1 
     *
     * This class manages all Python modules of an EKA2L1 instance.
     * This class also manages all panic, system call and reschedule hooks,
     * allowing users to discover and work with high-level interface of
     * EKA2L1
     */
    class script_manager {
        std::unordered_map<std::string, pybind11::module> modules;

        std::vector<panic_func> panic_functions;
        std::vector<svc_func> svc_functions;
        std::vector<pybind11::function> reschedule_functions;

        pybind11::scoped_interpreter interpreter;

        system *sys;
        std::mutex smutex;

    protected:
        bool call_module_entry(const std::string &module);

    public:
        script_manager() {}
        script_manager(system *sys);

        bool import_module(const std::string &path);

        void call_panics(const std::string &panic_cage, int err_code);
        void call_svcs(int svc_num);
        void call_reschedules();

        void register_panic(const std::string &panic_cage, pybind11::function &func);
        void register_svc(int svc_num, pybind11::function &func);
        void register_reschedule(pybind11::function &func);
    };
}