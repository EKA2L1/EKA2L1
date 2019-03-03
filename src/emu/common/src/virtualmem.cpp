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

#include <common/platform.h>
#include <common/virtualmem.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#elif EKA2L1_PLATFORM(UNIX)
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#endif

namespace eka2l1::common {
    void *map_memory(const std::size_t size) {
#if EKA2L1_PLATFORM(WIN32)
        return VirtualAlloc(nullptr, size,
            MEM_RESERVE, PAGE_NOACCESS);
#else
        return mmap(nullptr, size, PROT_NONE,
            MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
    }

    bool unmap_memory(void *ptr, const std::size_t size) {
#if EKA2L1_PLATFORM(WIN32)
        const auto result = VirtualFree(ptr, 0, MEM_RELEASE);

        if (!result) {
#else
        const int result = munmap(ptr, size);

        if (result == -1) {
#endif
            return false;
        }

        return true;
    }

    bool commit(void *ptr, const std::size_t size, const prot commit_prot) {
#if EKA2L1_PLATFORM(WIN32)
        DWORD oldprot = 0;

        const auto res = VirtualAlloc(ptr, size, MEM_COMMIT,
            translate_protection(commit_prot));

        if (!res) {
#else
        const int result = mprotect(ptr, size, translate_protection(commit_prot));

        if (result == -1) {
#endif
            return false;
        }

        return true;
    }

    bool decommit(void *ptr, const std::size_t size) {
#if EKA2L1_PLATFORM(WIN32)
        const auto res = VirtualFree(ptr, size, MEM_DECOMMIT);

        if (!res) {
#else
        const auto result = mprotect(ptr, size, PROT_NONE);

        if (result == -1) {
#endif
            return false;
        }

        return true;
    }

    bool change_protection(void *ptr, const std::size_t size,
        const prot new_prot) {
#if EKA2L1_PLATFORM(WIN32)
        DWORD oldprot = 0;
        const auto res = VirtualProtect(ptr, size,
            translate_protection(new_prot), &oldprot);

        if (!res) {
#else
        const int result = mprotect(ptr, size, translate_protection(new_prot));

        if (result == -1) {
#endif
            return false;
        }

        return true;
    }

    bool is_memory_wx_exclusive() {
#if EKA2L1_PLATFORM(UWP) || EKA2L1_PLATFORM(IOS)
        return true;
#else
        return false;
#endif
    }

    int get_host_page_size() {
#if EKA2L1_PLATFORM(WIN32)
        SYSTEM_INFO system_info = {};
        GetSystemInfo(&system_info);

        return system_info.dwPageSize;
#else
        return sysconf(_SC_PAGESIZE);
#endif
    }

    void *map_file(const std::string &file_name, const prot perm, const std::size_t size) {
#if EKA2L1_PLATFORM(WIN32)
        DWORD desired_access = 0;
        DWORD share_mode = 0;
        DWORD page_type = 0;
        DWORD map_type = 0;
        DWORD open_type = 0;

        switch (perm) {
        case prot::read: {
            desired_access = GENERIC_READ;
            share_mode = FILE_SHARE_READ;
            page_type = PAGE_READONLY;
            open_type = OPEN_ALWAYS;
            map_type = FILE_MAP_READ;

            break;
        }

        case prot::write: {
            desired_access = GENERIC_WRITE;
            share_mode = FILE_SHARE_WRITE;
            page_type = PAGE_READWRITE;
            open_type = CREATE_ALWAYS;
            map_type = FILE_MAP_WRITE;

            break;
        }

        case prot::read_write: {
            desired_access = GENERIC_WRITE | GENERIC_READ;
            share_mode = FILE_SHARE_WRITE | FILE_SHARE_READ;
            page_type = PAGE_READWRITE;
            open_type = CREATE_ALWAYS;
            map_type = FILE_MAP_WRITE | FILE_MAP_READ;

            break;
        }

        default: {
            return nullptr;
        }
        }

        HANDLE file_handle = CreateFileA(file_name.c_str(), desired_access, share_mode,
            NULL, open_type, NULL, NULL);

        if (file_handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        HANDLE map_file_handle = CreateFileMappingA(file_handle, NULL, page_type,
            size >> 32, static_cast<DWORD>(size), file_name.c_str());

        if (!map_file_handle || map_file_handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        auto map_ptr = MapViewOfFile(map_file_handle, map_type,
            0, 0, 0);
#else
        int open_mode = 0;
        const int prot_mode = translate_protection(perm);
        
        switch (perm) {
        case prot::read: {
            open_mode = O_RDONLY;
            break;
        }

        case prot::write: {
            open_mode = O_WRONLY | O_CREAT;
            break;
        }

        case prot::read_write: {
            open_mode = O_RDWR | O_CREAT;
            break;
        }

        default: {
            return nullptr;
        }
        }

        int file_handle = open(file_name.c_str(), open_mode);

        if (file_handle == -1) {
            return nullptr;
        }

        std::size_t map_size = size;

        if (perm == prot::read || (perm == prot::read_write && size == 0)) {
            struct stat file_stat;
            stat(file_name.c_str(), &file_stat);

            map_size = file_stat.st_size;
        }

        auto map_ptr = mmap(nullptr, map_size, prot_mode, MAP_PRIVATE,
            file_handle, 0);
#endif

        return map_ptr;
    }

    bool unmap_file(void *ptr) {
#if EKA2L1_PLATFORM(WIN32)
        UnmapViewOfFile(ptr);
#endif

        return true;
    }
}
