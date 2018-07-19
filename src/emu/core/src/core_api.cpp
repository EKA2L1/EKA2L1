/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <core/core.h>
#include <core/core_api.h>
#include <vector>

using sys_ptr = std::unique_ptr<eka2l1::system>;
std::vector<sys_ptr> syses;

using namespace eka2l1;

int create_symbian_system(int win_type, int render_type, int cpu_type) {
    syses.push_back(std::make_unique<eka2l1::system>(win_type == GLFW ? driver::window_type::glfw : driver::window_type::glfw,
        render_type == OPENGL ? driver::driver_type::opengl : driver::driver_type::opengl,
        cpu_type == CPU_UNICORN ? arm::jitter_arm_type::unicorn : arm::jitter_arm_type::dynarmic));

    return (int)syses.size();
}

int init_symbian_system(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys->init();

    return 0;
}

int load_process(int sys, unsigned int id) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    process *p = symsys->load(id);

    if (!p) {
        return -2;
    }

    return 0;
}

int run_symbian_system(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys->loop();

    return 0;
}

int shutdown_symbian_system(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys->shutdown();

    return 0;
}

int free_symbian_system(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys.reset();

    syses.erase(syses.begin() + sys - 1);

    return 0;
}

int get_total_app_installed(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    return symsys->total_app();
}

// Get the first app installed. If name is nullptr, this set the name length
int get_app_installed(int sys, int idx, char *name, int *name_len, unsigned int *id) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    auto infos = symsys->app_infos();
    auto need = infos[idx];

    if (name_len) {
        *name_len = need.name.size();
    }

    if (name) {
        memcpy(name, need.name.data(), (*name_len) * 2);
    }

    if (id) {
        *id = need.id;
    }

    return 0;
}

// Mount the system with a real folder
int mount_symbian_system(int sys, const char *drive, const char *real_path) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];

    symsys->mount(strncmp(drive, "C:", 2) == 0 ? 
        availdrive::c : (strncmp(drive, "E:", 2) == 0 ? availdrive::e : availdrive::z), 
        real_path);

    return 0;
}

int loop_system(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];

    while (!symsys->should_exit())
        symsys->loop();
}

// Load the ROM into symbian memory
int load_rom(int sys, const char *path) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];

    if (!path)
        return -1;

    if (symsys->load_rom(path)) {
        return 0;
    }

    return -1;
}

// Which Symbian version is gonna be used. Get it.
int get_current_symbian_use(int sys, unsigned int *ver) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    *ver = (symsys->get_symbian_version_use() == epocver::epoc6) ? EPOC6 : EPOC9;

    return 0;
}

// Which Symbian version is gonna be used. Set it.
int set_current_symbian_use(int sys, unsigned int ver) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys->set_symbian_version_use((ver == EPOC6) ? epocver::epoc6 : epocver::epoc9);

    return 0;
}

int install_sis(int sys, int drive, const char* path) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys->install_package(std::u16string(path, path + strlen(path)), drive);

    return 0;
}

int reinit_system(int sys) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    symsys->reset();

	return 0;
}

int install_rpkg(int sys, const char *path) {
    if (sys > syses.size()) {
        return -1;
    }

    sys_ptr &symsys = syses[sys - 1];
    bool res = symsys->install_rpkg(path);

    return res ? 0: -2;
}