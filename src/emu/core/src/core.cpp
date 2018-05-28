#include <core.h>
#include <process.h>

#include <common/log.h>
#include <disasm/disasm.h>
#include <loader/eka2img.h>

namespace eka2l1 {
    void system::init() {
        log::setup_log(nullptr);

        // Initialize all the system that doesn't depend on others first
        timing.init();
        mem.init();

        cpu = arm::create_jitter(&timing, &mem, &asmdis, jit_type);

        io.init(&mem);
        mngr.init(&io);
        asmdis.init(&mem);
		hlelibmngr.init(&io, &mem, ver);
        kern.init(&timing, &mngr, &mem, &hlelibmngr, cpu.get());
    }

    void system::load(uint64_t id) {
		crr_process = kern.spawn_new_process(id);
        crr_process->run();
    }

    int system::loop() {
        auto prepare_reschedule = [&]() {
            cpu->prepare_rescheduling();
            reschedule_pending = true;
        };

        if (kern.crr_thread() == nullptr) {
            timing.idle();
            timing.advance();
            prepare_reschedule();
        } else {
            timing.advance();
            cpu->run();
        }

        kern.reschedule();
        reschedule_pending = false;

        return 1;
    }

    bool system::install_package(std::u16string path, uint8_t drv) {
        return mngr.get_package_manager()->install_package(path, drv);
    }

    bool system::load_rom(const std::string& path) {
        auto romf_res = loader::load_rom(path);

        if (!romf_res) {
            return false;
        }

        romf = romf_res.value();
        io.set_rom_cache(&romf);

        bool res1 = mem.load_rom(path);

        if (!res1) {
            return false;
        }

        return true;
    }

    void system::shutdown() {
        timing.shutdown();
        kern.shutdown();
        mem.shutdown();
        asmdis.shutdown();
    }

	void system::mount(availdrive drv, std::string path) {
		io.mount(((drv == availdrive::c) ? "C:" : "E:"), path);
	}
}
