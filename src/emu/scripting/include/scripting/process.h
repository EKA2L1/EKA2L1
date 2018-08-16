#pragma once

#include <core/kernel/process.h>
#include <core/page_table.h>

#include <memory>
#include <vector>

namespace eka2l1::scripting {
    class process {
        eka2l1::process_ptr process_handle;

    public:
        process(uint64_t handle);

        bool read_process_memory(const size_t addr, std::vector<char> &buffer, const size_t size);
        bool write_process_memory(const size_t addr, std::vector<char> buffer);

        //std::unique_ptr<eka2l1::page> get_page_info(size_t addr);
    };
}