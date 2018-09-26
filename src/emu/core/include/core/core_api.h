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

#define GLFW 0x50535054

#define OPENGL 0x10000000

#define VFS_ATTRIB_HIDDEN 0x100
#define VFS_ATTRIB_WRITE_PROTECTED 0x200
#define VFS_ATTRIB_INTERNAL 0x400
#define VFS_ATTRIB_REMOVEABLE 0x800

#define VFS_MEDIA_PHYSICAL 0x2
#define VFS_MEDIA_ROM 0x4

#define VFS_DRIVE_A 0x00
#define VFS_DRIVE_B 0x01
#define VFS_DRIVE_C 0x02
#define VFS_DRIVE_D 0x03
#define VFS_DRIVE_E 0x04
#define VFS_DRIVE_F 0x05
#define VFS_DRIVE_Z 0x19

#ifdef _MSC_VER
#ifdef EKA2L1_API_EXPORT
#define EKA2L1_API __declspec(dllexport)
#else
#define EKA2L1_API __declspec(dllimport)
#endif
#else
#define EKA2L1_API
#endif

// Support Lazarus Pascal GUI and other wants to use EKA2L1 as API
extern "C" {
EKA2L1_API int create_symbian_system(int win_type, int render_type, int cpu_type);

EKA2L1_API int init_symbian_system(int sys);
EKA2L1_API int load_process(int sys, unsigned int id);

EKA2L1_API int run_symbian_system(int sys);
EKA2L1_API int shutdown_symbian_system(int sys);

EKA2L1_API int get_total_app_installed(int sys);

// Get the first app installed. If name is nullptr, this set the name length
EKA2L1_API int get_app_installed(int sys, int idx, char *name, int *name_len, unsigned int *id);

// Mount the system with a real folder
EKA2L1_API int mount_symbian_system(int sys, const int attrib, const int media_type, const int drive,
    const char *real_path);

// Load the ROM into symbian memory
EKA2L1_API int load_rom(int sys, const char *path);

// Which Symbian version is gonna be used. Get it.
EKA2L1_API int get_current_symbian_use(int sys, unsigned int *ver);

// Which Symbian version is gonna be used. Set it.
EKA2L1_API int set_current_symbian_use(int sys, unsigned int ver);

EKA2L1_API int loop_system(int sys);

EKA2L1_API int free_symbian_system(int sys);

EKA2L1_API int install_sis(int sys, int drive, const char *path);

EKA2L1_API int reinit_system(int sys);

EKA2L1_API int install_rpkg(int sys, const char *path);
}