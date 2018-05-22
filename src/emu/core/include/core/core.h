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
#include <optional>
#include <tuple>

namespace eka2l1 {
	using symversion = std::pair<uint8_t, uint8_t>;

    class system {
        std::shared_ptr<process> crr_process;
		std::mutex mut;

		std::unique_ptr<hle::lib_manager> hlelibmngr;
		arm::jitter cpu;
		
		memory_system mem;
		kernel_system kern;
		timing_system timing;

		manager_system mngr;
		io_system io;

		disasm asmdis;

		loader::rom romf;

		bool reschedule_pending;

		std::map<symversion, std::string> sids_path;
		std::optional<symversion> current_version;


    public:
		system()
			: hlelibmngr(nullptr) {}

		void add_sid(uint8_t major, uint8_t minor, const std::string& path);

		void set_symbian_version_use(uint8_t major, uint8_t minor);
		std::optional<symversion> get_symbian_version_use() const;

        void init();
        void load(const std::string &name, uint64_t id, const std::string &path);
        int loop();
        void shutdown();

		bool install_package(std::u16string path, uint8_t drv);
		bool load_rom(const std::string& path);
    };
}
