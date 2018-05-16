#pragma once

#include <core_kernel.h>
#include <core_mem.h>
#include <core_timing.h>
#include <hle/libmanager.h>
#include <arm/jit_factory.h>

#include <functional>
#include <memory>
#include <mutex>
#include <process.h>

namespace eka2l1 {
    class system {
        std::shared_ptr<process> crr_process;
		std::mutex mut;

		std::unique_ptr<hle::lib_manager> mngr;
		arm::jitter cpu;
		
		memory mem;
		kernel_system kern;
		timing_system timing;

		disasm asm;

		bool reschedule_pending;

	protected:

		void prepare_reschedule();
		void reschedule();

    public:
        void init();
        void load(const std::string &name, uint64_t id, const std::string &path);
        int loop();
        void shutdown();
    }
}
