#pragma once

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <common/types.h>

#include <map>
#include <mutex>
#include <tuple>
#include <unordered_map>

namespace eka2l1 {
    class system;
    struct ipc_msg;

    namespace kernel {
        class thread;
    }
}

namespace eka2l1::manager {
    using panic_func = std::pair<std::string, pybind11::function>;
    using svc_func = std::tuple<int, int, pybind11::function>;

    using func_list = std::vector<pybind11::function>;

    /*! \brief A manager for all custom Python scripts of EKA2L1 
     *
     * This class manages all Python modules of an EKA2L1 instance.
     * This class also manages all panic, system call and reschedule hooks,
     * allowing users to discover and work with high-level interface of
     * EKA2L1
     */
    class script_manager {
        std::unordered_map<std::string, pybind11::module> modules;

        std::unordered_map<uint32_t, func_list> breakpoints;
        std::unordered_map<std::string, std::unordered_map<std::uint32_t, func_list>> breakpoints_patch;
        std::unordered_map<std::string, std::map<std::uint64_t, func_list>> ipc_functions;

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
        void call_svcs(int svc_num, int time);
        void call_breakpoints(const uint32_t addr);
        void call_ipc_send(const std::string &server_name, const int opcode, const std::uint32_t arg0,
            const std::uint32_t arg1, const std::uint32_t arg2, const std::uint32_t arg3,
            const std::uint32_t flags, kernel::thread *callee);

        void call_ipc_complete(const std::string &server_name, const int opcode,
            ipc_msg *msg);

        void call_reschedules();

        void register_panic(const std::string &panic_cage, pybind11::function &func);
        void register_svc(int svc_num, int time, pybind11::function &func);
        void register_reschedule(pybind11::function &func);
        void register_library_hook(const std::string &name, const uint32_t ord, pybind11::function &func);
        void register_breakpoint(const uint32_t addr, pybind11::function &func);
        void register_ipc(const std::string &server_name, const int opcode, const int invoke_when, pybind11::function &func);

        void patch_library_hook(const std::string &name, const std::vector<vaddress> exports);
    };
}
