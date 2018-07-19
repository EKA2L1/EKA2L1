#include <common/log.h>
#include <core/page_table.h>

namespace eka2l1 {
    page_table::page_table(int page_size)
        : page_size(page_size) {
        int total_page = 0xFFFFFFFF / page_size;

        pointers.resize(total_page);
        pages.resize(total_page);
    }

    void page_table::read(uint32_t addr, void *dest, int size) {
        if (addr >= global_data && addr <= ram_drive) {
            LOG_WARN("Access global data passthroughed on process page table.");
            return;
        }

        if (addr >= null_trap && addr <= local_data) {
            LOG_WARN("Accessing null trap region, exception won't be thrown");
            return;
        }   

        int page = addr / page_size;
        uint32_t page_addr = page * page_size;

        switch (pages[page].sts) {
        case page_status::free:
        case page_status::reserved:
            LOG_WARN("Reading free / reserved page at addr: 0x{:x}", addr);
            break;

        case page_status::committed:
            memcpy(dest, &((pointers[page]).get()[page_addr - addr]), size);
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
        uint32_t page_addr = page * page_size;

        switch (pages[page].sts) {
        case page_status::free:
        case page_status::reserved:
            LOG_WARN("Writing free / reserved page at addr: 0x{:x}", addr);
            break;

        case page_status::committed:
            memcpy(&((pointers[page]).get()[page_addr - addr]), src, size);
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
        uint32_t page_addr = page * page_size;

        switch (pages[page].sts) {
        case page_status::free:
        case page_status::reserved:
            LOG_WARN("Reading free / reserved page at addr: 0x{:x}", addr);
            return nullptr;

        case page_status::committed:
            return &((pointers[page]).get()[page_addr - addr]);
        }

        return nullptr;        
    }
}