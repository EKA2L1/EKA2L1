#pragma once

#include <core_timing.h>
#include <core_mem.h>
#include <core_kernel.h>

#include <functional>

namespace eka2l1 {
    namespace core {
        void init();
        void load(const std::string& name, uint64_t id, const std::string& path);
        int loop();
        void shutdown();
    }
}
