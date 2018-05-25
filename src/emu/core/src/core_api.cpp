#include <core_api.h>
#include <core.h>
#include <vector>

using sys_ptr = std::unique_ptr<eka2l1::system>;
std::vector<sys_ptr> syses;

using namespace eka2l1;

int create_symbian_system(int cpu_type) {
	syses.push_back(std::make_unique<eka2l1::system>(cpu_type == CPU_UNICORN ? arm::jitter_arm_type::unicorn : arm::jitter_arm_type::unicorn));
	return (int)syses.size();
}

int init_symbian_system(int sys) {
	if (sys > syses.size()) {
		return -1;
	}
	
	sys_ptr& symsys = syses[sys - 1];
	symsys->init();

	return 0;
}

int load_process(int sys, unsigned int id) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	symsys->load(id);

	return 0;
}

int run_symbian_system(int sys) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	symsys->loop();

	return 0;
}

int shutdown_symbian_system(int sys) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	symsys->shutdown();

	symsys.reset();
	syses.pop_back();

	return 0;
}

int get_total_app_installed(int sys) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	return symsys->total_app();
}

// Get the first app installed. If name is nullptr, this set the name length
int get_app_installed(int sys, int idx, char* name, int* name_len, unsigned int* id) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	auto infos = symsys->app_infos();
	auto need = infos[idx];
	
	if (name_len) {
		*name_len = need.name.size();
	}

	if (name) {
		memcpy(name, need.name.data(), *name_len);
	}

	if (id) {
		*id = need.id;
	}

	return 0;
}

// Mount the system with a real folder
int mount_symbian_system(int sys, const char* drive, const char* real_path) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	// Mount
	return 0;
}

// Load the ROM into symbian memory
int load_rom(int sys, const char* path) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	
	if (!path)
		return -1;
	
	if (symsys->load_rom(path)) {
		return 0;
	}

	return -1;
}

// Which Symbian version is gonna be used. Get it.
int get_current_symbian_use(int sys, unsigned int* ver) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	*ver = (symsys->get_symbian_version_use() == epocver::epoc6) ? EPOC6 : EPOC9;

	return 0;
}

// Which Symbian version is gonna be used. Set it.
int set_current_symbian_use(int sys, unsigned int ver) {
	if (sys > syses.size()) {
		return -1;
	}

	sys_ptr& symsys = syses[sys - 1];
	symsys->set_symbian_version_use((ver == EPOC6) ? eka2l1::epocver::epoc6 : eka2l1::epocver::epoc9);

	return 0;
}