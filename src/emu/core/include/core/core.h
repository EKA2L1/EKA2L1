#pragma once

#include <core_timing.h>
#include <core_mem.h>
#include <core_kernel.h>

#include <functional>

namespace eka2l1 {
    namespace core {
        // Register gui rendering
        using gui_rendering_func = std::function<void()>;

        void init();
        void load(const std::string& name, uint64_t id, const std::string& path);

        void register_gui_rendering(gui_rendering_func func);

        int loop();

        void shutdown();
    }
}
