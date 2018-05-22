#pragma once

#define CPU_UNICORN 0
#define CPU_DYNARMIC 1

// Support Lazarus Pascal GUI and other wants to use EKA2L1 as API
extern "C" {
    int create_symbian_system(int cpu_type);

	int init_symbian_system(int sys);
    int load_process(int sys, unsigned int id);

    int run_symbian_system(int sys);
    int shutdown_symbian_system(int sys);

    int get_total_app_installed(int sys);

    // Get the first app installed. If name is nullptr, this set the name length
    int get_app_installed(int sys, int idx, char* name, int* name_len, unsigned int* id);

    // Mount the system with a real folder
    int mount_symbian_system(int sys, const char* drive, const char* real_path);

	// Load the ROM into symbian memory
    int load_rom(int sys, const char* path);

	// Which Symbian version is gonna be used. Get it.
	int get_current_symbian_use(int sys, unsigned char* major, unsigned char* minor);

	// Which Symbian version is gonna be used. Set it.
	int set_current_symbian_use(int sys, unsigned char major, unsigned char minor);

	// Add the path to one of the SID database
	int symbian_system_add_sid(int sys, unsigned char major, unsigned char minor, const char* path);
}