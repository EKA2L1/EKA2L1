#pragma once

#include <core_kernel.h>
#include <core_mem.h>
#include <core_timing.h>
#include <vfs.h>
#include <hle/libmanager.h>
#include <arm/jit_factory.h>
#include <disasm/disasm.h>
#include <manager/manager.h>
#include <manager/package_manager.h>
#include <loader/rom.h>

#include <functional>
#include <memory>
#include <mutex>
#include <process.h>
#include <optional>
#include <tuple>

namespace eka2l1 {
	enum class availdrive {
		c,
		e
	};

    class system {
        process* crr_process;
		std::mutex mut;

		std::unique_ptr<hle::lib_manager> hlelibmngr;
		arm::jitter cpu;
		arm::jitter_arm_type jit_type;
		
		memory_system mem;
		kernel_system kern;
		timing_system timing;

		manager_system mngr;
		io_system io;

		disasm asmdis;

		loader::rom romf;

		bool reschedule_pending;

		epocver ver = epocver::epoc9;

    public:
		system(arm::jitter_arm_type jit_type = arm::jitter_arm_type::unicorn)
			: hlelibmngr(nullptr), jit_type(jit_type) {}

		void set_symbian_version_use(const epocver ever) {
			ver = ever;
		}

		epocver get_symbian_version_use() const {
			return ver;
		}

        void init();
        void load(uint64_t id);
        int loop();
        void shutdown();

		void mount(availdrive drv, std::string path);

		bool install_package(std::u16string path, uint8_t drv);
		bool load_rom(const std::string& path);

		uint32_t total_app() {
			return mngr.get_package_manager()->app_count();
		}

		std::vector<manager::app_info> app_infos() {
			return mngr.get_package_manager()->get_apps_info();
		}
    };
}
