#include <core.h>
#include <process.h>

#include <common/log.h>
#include <disasm/disasm.h>
#include <loader/eka2img.h>

namespace eka2l1 {
	void system::add_sid(uint8_t major, uint8_t minor, const std::string& path) {
		sids_path.insert(std::make_pair(
			std::make_pair(major, minor),
			path)
		);
	}

	void system::set_symbian_version_use(uint8_t major, uint8_t minor) {
		current_version.emplace(std::make_pair(major, minor));
	}

	std::optional<symversion> system::get_symbian_version_use() const {
		return current_version.value();
	}

    void system::init() {
		symversion ver;
		ver.first = 9;
		ver.second = 4;

        log::setup_log(nullptr);

        // Initialize all the system that doesn't depend on others first
        timing.init();
        mem.init();

		if (current_version) {
			ver = current_version.value();
		}

		hlelibmngr = std::make_unique<hle::lib_manager>(sids_path[ver]);

        cpu = arm::create_jitter(&timing, &mem, &asmdis, arm::jitter_arm_type::unicorn);

        io.init(&mem);
        mngr.init(&io);
        asmdis.init(&mem);
        kern.init(&timing, cpu.get());
    }

    void system::load(const std::string &name, uint64_t id, const std::string &path) {
        auto img = loader::parse_eka2img(path);

        if (!img) {
            LOG_CRITICAL("This is not what i expected! Fake E32Image!");
            return;
        }

        loader::eka2img &real_img = img.value();
        hlelibmngr = std::make_unique<hle::lib_manager>("db.yml");

        bool succ = loader::load_eka2img(real_img, &mem, *hlelibmngr);

        if (!succ) {
            LOG_CRITICAL("Unable to load EKA2Img!");
            return;
        }

        crr_process = std::make_shared<process>(&kern, &mem, id, name, 
            real_img.rt_code_addr + real_img.header.entry_point,
            real_img.header.heap_size_min, real_img.header.heap_size_max,
            real_img.header.stack_size);

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
}
