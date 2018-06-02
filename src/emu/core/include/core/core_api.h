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
#pragma once

#define CPU_UNICORN 0
#define CPU_DYNARMIC 1

#define EPOC6 0x65243205
#define EPOC9 0x65243209

// Support Lazarus Pascal GUI and other wants to use EKA2L1 as API
extern "C" {
int create_symbian_system(int cpu_type);

int init_symbian_system(int sys);
int load_process(int sys, unsigned int id);

int run_symbian_system(int sys);
int shutdown_symbian_system(int sys);

int get_total_app_installed(int sys);

// Get the first app installed. If name is nullptr, this set the name length
int get_app_installed(int sys, int idx, char *name, int *name_len, unsigned int *id);

// Mount the system with a real folder
int mount_symbian_system(int sys, const char *drive, const char *real_path);

// Load the ROM into symbian memory
int load_rom(int sys, const char *path);

// Which Symbian version is gonna be used. Get it.
int get_current_symbian_use(int sys, unsigned int *ver);

// Which Symbian version is gonna be used. Set it.
int set_current_symbian_use(int sys, unsigned int ver);
}
