#pragma once

#include <core_kernel.h>
#include <core_mem.h>
#include <core_timing.h>
#include <vfs.h>
#include <hle/libmanager.h>
#include <arm/jit_factory.h>
#include <disasm/disasm.h>
#include <manager/manager.h>
#include <loader/rom.h>

#include <functional>
#include <memory>
#include <mutex>
#include <process.h>

namespace eka2l1 {
    class system {
        std::shared_ptr<process> crr_process;
		std::mutex mut;

		std::unique_ptr<hle::lib_manager> hlelibmngr;
		arm::jitter cpu;
		
		memory mem;
		kernel_system kern;
		timing_system timing;

		manager_system mngr;
		io_system io;

		disasm asmdis;

		loader::rom romf;

		bool reschedule_pending;

	protected:

		void prepare_reschedule();
		void reschedule();

    public:
		system()
			: hlelibmngr(nullptr) {}

        void init();
        void load(const std::string &name, uint64_t id, const std::string &path);
        int loop();
        void shutdown();

		bool install_package(std::u16string path, uint8_t drv);
		bool load_rom(const std::string& path);
    };
}
