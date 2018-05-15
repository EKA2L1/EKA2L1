#pragma once

#include <core_kernel.h>
#include <core_mem.h>
#include <core_timing.h>
#include <hle/libmanager.h>

#include <functional>
#include <memory>
#include <mutex>

namespace eka2l1 {
    // An app ...
    class app {
        std::shared_ptr<process> crr_process;
		std::mutex mut;

		std::unique_ptr<hle::lib_manager> mngr;

    public:
        void init();
        void load(const std::string &name, uint64_t id, const std::string &path);
        int loop();
        void shutdown();
    }
}
