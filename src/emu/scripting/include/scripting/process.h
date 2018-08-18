#pragma once

#include <core/kernel/process.h>
#include <core/page_table.h>

#include <memory>
#include <vector>

namespace eka2l1::scripting {
    class thread;

    class process {
        eka2l1::process_ptr process_handle;

    public:
        process(uint64_t handle);

        bool read_process_memory(const size_t addr, std::vector<char> &buffer, const size_t size);
        bool write_process_memory(const size_t addr, std::vector<char> buffer);

        std::string get_executable_path();
        std::string get_name();

        std::vector<std::unique_ptr<eka2l1::scripting::thread>> get_thread_list();
    };

    std::vector<std::unique_ptr<eka2l1::scripting::process>> get_process_list();
}