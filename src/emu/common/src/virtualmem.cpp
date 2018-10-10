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

#include <common/virtualmem.h>

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#endif

namespace eka2l1::common {
    void *map_memory(const std::size_t size) {
#ifdef WIN32
        return VirtualAlloc(nullptr, size,
            MEM_RESERVE, PAGE_NOACCESS);
#else
        return mmap(nullptr, size, PROT_NONE,
            MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
    }

    bool  unmap_memory(void *ptr, const std::size_t size) {
#ifdef WIN32
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
#ifdef WIN32
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
#ifdef WIN32
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
#ifdef WIN32
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
    
    int get_host_page_size() {
#ifdef WIN32
        SYSTEM_INFO system_info = {};
        GetSystemInfo(&system_info);

        return system_info.dwPageSize;
#else
        return sysconf(_SC_PAGESIZE);
#endif
    }

    void *map_file(const std::string &file_name) {
#ifdef WIN32
        HANDLE file_handle = CreateFileA(file_name.c_str(), GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_ALWAYS, NULL, NULL);

        if (!file_handle) {
            return false;
        }

        HANDLE map_file_handle = CreateFileMappingA(file_handle, NULL, PAGE_READONLY,
            0, 0, file_name.c_str());

        if (!map_file_handle || map_file_handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        auto map_ptr = MapViewOfFile(map_file_handle, FILE_MAP_READ,
            0, 0, 0);
#else
        int file_handle = open(file_name.c_str(), O_RDONLY);

        if (file_handle == -1) {
            return nullptr;
        }

        struct stat file_stat;
        stat(file_name.c_str(), &file_stat);

        auto map_ptr = mmap(nullptr, file_stat.st_size, PROT_READ, MAP_PRIVATE,
            file_handle, 0);
#endif

        return map_ptr;
    }

    bool unmap_file(void *ptr) {
#ifdef WIN32
        UnmapViewOfFile(ptr);
#endif

        return true;
    }
}