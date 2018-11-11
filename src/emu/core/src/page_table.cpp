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

#include <common/log.h>
#include <core/page_table.h>

namespace eka2l1 {
    page_table::page_table(int page_size)
        : page_size(page_size) {
        page clear = { 0, page_status::free, prot::none };
        std::fill(pages.begin(), pages.end(), clear);
        std::fill(pointers.begin(), pointers.end(), nullptr);
    }

    std::array<page, page_table_number_entries> &page_table::get_pages() {
        const std::lock_guard<std::mutex> guard(mut);
        return pages;
    }

    std::array<std::uint8_t*, page_table_number_entries> &page_table::get_pointers() {
        const std::lock_guard<std::mutex> guard(mut);
        return pointers;
    }

    void page_table::read(uint32_t addr, void *dest, int size) {
        int page = addr / page_size;

        std::fill(reinterpret_cast<std::uint8_t*>(dest), reinterpret_cast<std::uint8_t*>(dest) + size,
            0);

        switch (get_pages()[page].sts) {
        case page_status::free:
        case page_status::reserved:
            break;

        case page_status::committed:
            memcpy(dest, &((pointers[page])[addr % page_size]), size);
        }

        return;
    }

    void page_table::write(uint32_t addr, void *src, int size) {
        if (addr >= global_data && addr <= ram_drive) {
            LOG_WARN("Access global data passthroughed on process page table.");
            return;
        }

        if (addr >= null_trap && addr <= local_data) {
            LOG_WARN("Accessing null trap region, exception won't be thrown");
            return;
        }

        int page = addr / page_size;

        switch (pages[page].sts) {
        case page_status::free:
        case page_status::reserved:
            LOG_WARN("Writing free / reserved page at addr: 0x{:x}", addr);
            break;

        /* Maybe get an invalid data when the main thread is writing new page status and 
           this threading is reading (only happens with shared and ram code).
        
           Block all pointers modification.
        */
        case page_status::committed:
            memcpy(&((get_pointers()[page])[addr % page_size]), src, size);
        }

        return;
    }

    void *page_table::get_ptr(uint32_t addr) {
        if (addr >= global_data && addr <= ram_drive) {
            LOG_WARN("Access global data passthroughed on process page table.");
            return nullptr;
        }

        if (addr >= null_trap && addr <= local_data) {
            LOG_WARN("Accessing null trap region, exception won't be thrown");
            return nullptr;
        }

        int page = addr / page_size;

        switch (get_pages()[page].sts) {
        case page_status::free:
        case page_status::reserved:
            LOG_WARN("Reading free / reserved page at addr: 0x{:x}", addr);
            return nullptr;

        /* This time, can't gurantee memory read/write safe, since other thread may 
           write async with the JIT. */
        case page_status::committed:
            return &((get_pointers()[page])[addr % page_size]);
        }

        return nullptr;        
    }
}