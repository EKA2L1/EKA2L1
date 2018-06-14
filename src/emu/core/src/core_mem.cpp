/*
 * Copyright (c) 2018 EKA2L1 Team / Vita3K TEam
 * 
 * This file is part of EKA2L1 project / Vita3K emulator project
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
#include <common/algorithm.h>
#include <common/log.h>
#include <common/cvt.h>
#include <core/ptr.h>

#include <algorithm>
#include <cstdint>

#include <functional>
#include <memory>
#include <vector>

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <errno.h>

namespace eka2l1 {
    void _free_mem(uint8_t *dt) {
#ifndef WIN32
        munmap(dt, common::GB(1));
#else
        VirtualFree(dt, 0, MEM_RELEASE);
#endif
    }

    void memory_system::shutdown() {
        mem_pages.clear();
        memory.reset();
    }

    void memory_system::init() {
#ifdef WIN32
        SYSTEM_INFO system_info = {};
        GetSystemInfo(&system_info);

        page_size = system_info.dwPageSize;
#else
        page_size = sysconf(_SC_PAGESIZE);
#endif

        size_t len = common::GB(4);

#ifndef WIN32
        memory = mem(static_cast<uint8_t *>(mmap(nullptr, len, PROT_READ,
                         MAP_ANONYMOUS | MAP_PRIVATE, 0, 0)),
            _free_mem);
#else
        memory = mem(reinterpret_cast<uint8_t *>(VirtualAlloc(nullptr, len, MEM_RESERVE, PAGE_NOACCESS)), _free_mem);
#endif

        if (!memory) {
            LOG_CRITICAL("Allocating virtual memory for emulating failed!");
            return;
        } else {
            LOG_INFO("Virtual memory allocated: 0x{:x}", (size_t)memory.get());
        }

        mem_pages.resize(len / page_size);
        generations = 0;
    }

    ptr<void> memory_system::chunk_range(size_t beg_addr, size_t end_addr, size_t bottom, size_t top, size_t max_grow, prot cprot) {
        // Find the reversed memory with this max grow
        size_t page_count = (max_grow + page_size - 1) / page_size;

        size_t page_begin_off = (beg_addr + page_size - 1) / page_size;
        size_t page_end_off = (end_addr - page_size + 1) / page_size;

        const auto& page_begin = mem_pages.rbegin() + mem_pages.size() - page_end_off;
        const auto& page_end = mem_pages.rbegin() + mem_pages.size() - page_begin_off;

        page holder;

        auto& suitable_pages = std::search_n(page_begin, page_end, page_count, holder, 
            [](const auto& lhs, const auto& rhs) {
            return (lhs.sts == page_status::free) && (lhs.generation == 0);
        });

        uint32_t idx = mem_pages.rend() - suitable_pages - page_count;

        if (suitable_pages != mem_pages.rend()) {
            return chunk(idx * page_size, bottom, top, max_grow, cprot);
        }

        return ptr<void>(nullptr);
    }

    // Create a new chunk with specified address. Return base of chunk
    ptr<void> memory_system::chunk(address addr, size_t bottom, size_t top, size_t max_grow, prot cprot) {
        max_grow -= max_grow % page_size;

        if (bottom % page_size != 0) {
            bottom += bottom % page_size;
        }

        if (top % page_size != 0) {
            top += top % page_size;
        }

        top = common::min(top, max_grow);

        size_t page_begin_off = (addr + page_size - 1) / page_size;
        size_t page_end_off = (addr + max_grow - 1 + page_size) / page_size;
        
        const auto& page_begin = mem_pages.begin() + page_begin_off;
        const auto& page_end = mem_pages.begin() + page_end_off;

        const size_t count = page_end_off - page_begin_off;

        for (auto ite = page_begin; ite != page_end; ite++) {
            // If the page is not free, than either it's reserved or commited
            // We can not make a new chunk on those pages
            if (ite->sts != page_status::free) {
                return ptr<void>(nullptr);
            }
        }

        const gen generation = ++generations;

        // We commit them later, so as well as assigned there protect first
        page new_page = { generation, page_status::reserved, cprot };

        std::fill(page_begin, page_end, new_page);

        commit(addr + bottom, top - bottom);

        return ptr<void>(addr);
    }

    // Commit to page
    int memory_system::commit(ptr<void> addr, size_t size) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        auto& page_begin = mem_pages.begin() + beg;
        auto& page_end = mem_pages.begin() + end;

        prot nprot = page_begin->protection;

        for (page_begin; page_begin != page_end; page_begin++) {
            // Can commit on commited region or reserved region
            if (page_begin->sts == page_status::free) {
                return -1;
            }

            page_begin->sts = page_status::committed;
        }

#ifdef WIN32
        DWORD oldprot = 0;
        auto res = VirtualAlloc(ptr<void>(beg* page_size).get(this), count * page_size, MEM_COMMIT, translate_protection(nprot));

        if (!res) {
            return -1;
    }
#else
        mprotect(ptr<void>(beg).get(this), count * page_size, translate_protection(nprot));
#endif

        return 0;
    }

    int memory_system::decommit(ptr<void> addr, size_t size) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        auto& page_begin = mem_pages.begin() + beg;
        auto& page_end = mem_pages.begin() + end;

        for (page_begin; page_begin != page_end; page_begin++) {
            if (page_begin->sts == page_status::committed) {
                page_begin->sts = page_status::reserved;
            }
        }

#ifdef WIN32
        DWORD oldprot = 0;
        auto res = VirtualFree(ptr<void>(beg * page_size).get(this), count * page_size, MEM_DECOMMIT);

        if (!res) {
            return -1;
        }
#else
        mprotect(ptr<void>(beg).get(this), count * page_size, PROT_NONE);
#endif
        return 0;
    }

    // Change the prot of pages
    int memory_system::change_prot(ptr<void> addr, size_t size, prot nprot) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        auto& page_begin = mem_pages.begin() + beg;
        auto& page_end = mem_pages.begin() + end;

        for (page_begin; page_begin != page_end; page_begin++) {
            // Only a commited region can have a protection
            if (page_begin->sts != page_status::committed) {
                return -1;
            }

            page_begin->protection = nprot;
        }

#ifdef WIN32
        DWORD oldprot = 0;
        BOOL res = VirtualProtect(ptr<void>(beg * page_size).get(this), count * page_size, translate_protection(nprot), &oldprot);

        if (!res) {
            return -1;
        }
#else
        mprotect(ptr<void>(beg).get(this), count * page_size, translate_protection(nprot));
#endif

        return 0;
    }

    // Mark a chunk at addr as unusable
    int memory_system::unchunk(ptr<void> addr, size_t length) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + length - 1 + page_size) / page_size;

        size_t count = end - beg;

        auto& page_begin = mem_pages.begin() + beg;
        auto& page_end = mem_pages.begin() + end;

        for (page_begin; page_begin != page_end; page_begin++) {
            page_begin->sts = page_status::free;
            page_begin->protection = prot::none;
            page_begin->generation = 0;
        }

        return 0;
    }

    // Load the ROM into virtual memory, using ma
    bool memory_system::load_rom(address addr, const std::string &rom_path) {
        FILE *f = fopen(rom_path.c_str(), "rb");

        if (f == nullptr) {
            return false;
        }

        fseek(f, 0, SEEK_END);

        auto size = ftell(f);
        auto aligned_size = ((size / page_size) + 1) * (page_size);
        auto left = size;

        chunk(addr, 0, size, size, prot::read_write);

        fseek(f, 0, SEEK_SET);

        long buf_once = 0;

        ptr<void> start(addr);

        while (left) {
            buf_once = common::min(left, (long)100000);
            fread(start.get(this), 1, buf_once, f);
            start += (address)buf_once;

            left -= buf_once;
        }

        fclose(f);

        return true;
    }
}

