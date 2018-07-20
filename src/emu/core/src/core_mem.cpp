#include <common/algorithm.h>
#include <common/log.h>

#include <core/arm/jit_interface.h>
#include <core/core_mem.h>

#include <algorithm>

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <errno.h>

static void _free_mem(uint8_t *dt) {
#ifndef WIN32
    munmap(dt, common::GB(1));
#else
    VirtualFree(dt, 0, MEM_RELEASE);
#endif
}

namespace eka2l1 {
    void memory_system::init(arm::jitter &jit, uint32_t code_ram_addr) {
#ifdef WIN32
        SYSTEM_INFO system_info = {};
        GetSystemInfo(&system_info);

        page_size = system_info.dwPageSize;
#else
        page_size = sysconf(_SC_PAGESIZE);
#endif

        codeseg_addr = code_ram_addr;
        codeseg_pages.resize(0x10000000 / page_size);
        codeseg_pointers.resize(0x10000000 / page_size);

        global_pages.resize(codeseg_pages.size());
        global_pointers.resize(codeseg_pointers.size());

        cpu = jit.get();
    }

    bool memory_system::map_rom(uint32_t addr, const std::string &path) {
        rom_addr = addr;

#ifdef WIN32
        HANDLE rom_file_handle = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_ALWAYS, NULL, NULL);

        if (!rom_file_handle) {
            return false;
        }

        HANDLE rom_map_file_handle = CreateFileMappingA(rom_file_handle, NULL, PAGE_READONLY,
            0, 0, "MappingRom");

        if (!rom_map_file_handle || rom_map_file_handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        rom_map = MapViewOfFile(rom_map_file_handle, FILE_MAP_READ,
            0, 0, 0);
#else
        int rom_file_handle = open(path.c_str(), O_RDONLY);

        if (rom_file_handle == -1) {
            return false;
        }

        rom_map = mmap(nullptr, 0x3000000, PROT_READ, MAP_PRIVATE,
            rom_file_handle, 0);

        if (!rom_map) {
            return false;
        }
#endif
        LOG_TRACE("Rom mapped to address: 0x{:x}", reinterpret_cast<uint64_t>(rom_map));

        cpu->map_backing_mem(rom_addr, common::align(common::MB(41), page_size),
            reinterpret_cast<uint8_t *>(rom_map), prot::read_exec);

        return true;
    }

    void *memory_system::get_real_pointer(address addr) {
        if (addr >= codeseg_addr && addr < codeseg_addr + 0x10000000) {
            if (!codeseg_pointers[(addr - codeseg_addr) / page_size].get()) {
                return nullptr;
            }

            return static_cast<void *>(codeseg_pointers[(addr - codeseg_addr) / page_size].get() + (addr - codeseg_addr) % page_size);
        }

        if (addr >= rom_addr && addr < rom_addr + 0x10000000) {
            return &reinterpret_cast<uint8_t *>(rom_map)[addr - rom_addr];
        }

        if (addr >= global_data && addr < ram_drive) {
            if (!global_pointers[(addr - global_data) / page_size].get()) {
                return nullptr;
            }

            return static_cast<void *>(global_pointers[(addr - global_data) / page_size].get() + (addr - global_data) % page_size);
        }

        if (!current_page_table->pointers[addr / page_size].get()) {
            return nullptr;
        }

        return static_cast<void *>(current_page_table->pointers[addr / page_size].get() + addr % page_size);
    }

    // Create a new chunk with specified address. Return base of chunk
    ptr<void> memory_system::chunk(address addr, size_t bottom, size_t top, size_t max_grow, prot cprot) {
        max_grow -= max_grow % page_size;

        cpu->unmap_memory(addr, max_grow);

        if (bottom % page_size != 0) {
            bottom += bottom % page_size;
        }

        if (top % page_size != 0) {
            top += top % page_size;
        }

        top = common::min(top, max_grow);

        size_t page_begin_off = (addr + page_size - 1) / page_size;
        size_t page_end_off = (addr + max_grow - 1 + page_size) / page_size;

        decltype(global_pages)::iterator page_begin;
        decltype(page_begin) page_end;

        size_t len = common::GB(4);

        if (addr >= global_data && addr < ram_drive) {
            page_begin = global_pages.begin() + (page_begin_off - (global_data / page_size));
            page_end = global_pages.begin() + (page_end_off - (global_data / page_size));
        } else if (addr >= codeseg_addr && addr < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.begin() + (page_begin_off - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.begin() + (page_end_off - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.begin() + page_begin_off;
            page_end = current_page_table->pages.begin() + page_end_off;
        }

        const size_t count = page_end_off - page_begin_off;

        for (auto ite = page_begin; ite != page_end; ite++) {
            // If the page is not free, than either it's reserved or commited
            // We can not make a new chunk on those pages
            if (ite->sts != page_status::free) {
                return ptr<void>(nullptr);
            }

            ite->sts = page_status::reserved;
        }

        const gen generation = ++generations;

        // We commit them later, so as well as assigned there protect first
        page new_page = { generation, page_status::reserved, cprot };

        std::fill(page_begin, page_end, new_page);

        if (addr >= global_data && addr < ram_drive) {
#ifndef WIN32
            global_pointers[page_begin_off - (global_data / page_size)]
                = mem_ptr(
                    static_cast<uint8_t *>(mmap(nullptr, count * page_size, PROT_READ,
                        MAP_ANONYMOUS | MAP_PRIVATE, 0, 0)),
                    _free_mem);
#else
            global_pointers[page_begin_off - (global_data / page_size)]
                = mem_ptr(
                    static_cast<uint8_t *>(VirtualAlloc(nullptr, count * page_size,
                        MEM_RESERVE, PAGE_NOACCESS)),
                    _free_mem);
#endif

            for (size_t i = page_begin_off - (global_data / page_size) + 1; i < page_end_off - (global_data / page_size); i++) {
                global_pointers[i] = mem_ptr(global_pointers[i - 1].get() + page_size);
            }

            if (current_page_table) {
                std::copy(global_pages.begin() + page_begin_off - (global_data / page_size),
                    global_pages.begin() + page_end_off - (global_data / page_size), current_page_table->pages.begin() + page_begin_off);

                std::copy(global_pointers.begin() + page_begin_off - (global_data / page_size),
                    global_pointers.begin() + page_end_off - (global_data / page_size), current_page_table->pointers.begin() + page_begin_off);
            }

            cpu->map_backing_mem(addr, max_grow, global_pointers[page_begin_off - (global_data / page_size)].get(), cprot);
        } else if (addr >= ram_code_addr && addr < codeseg_addr + 0x10000000) {
#ifndef WIN32
            codeseg_pointers[page_begin_off - (ram_code_addr / page_size)]
                = mem_ptr(
                    static_cast<uint8_t *>(mmap(nullptr, count * page_size, PROT_READ,
                        MAP_ANONYMOUS | MAP_PRIVATE, 0, 0)),
                    _free_mem);
#else
            codeseg_pointers[page_begin_off - (ram_code_addr / page_size)]
                = mem_ptr(
                    reinterpret_cast<uint8_t *>(VirtualAlloc(nullptr, count * page_size,
                        MEM_RESERVE, PAGE_NOACCESS)),
                    _free_mem);
#endif
            for (size_t i = page_begin_off - (ram_code_addr / page_size) + 1; i < page_end_off - (ram_code_addr / page_size); i++) {
                codeseg_pointers[i] = mem_ptr(codeseg_pointers[i - 1].get() + page_size);
            }

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + page_begin_off - (codeseg_addr / page_size),
                    codeseg_pages.begin() + page_end_off - (codeseg_addr / page_size), current_page_table->pages.begin() + page_begin_off);

                std::copy(codeseg_pointers.begin() + page_begin_off - (codeseg_addr / page_size),
                    codeseg_pointers.begin() + page_end_off - (codeseg_addr / page_size), current_page_table->pointers.begin() + page_begin_off);
            }

            cpu->map_backing_mem(addr, max_grow, codeseg_pointers[page_begin_off - (codeseg_addr / page_size)].get(), cprot);
        } else {
#ifndef WIN32
            current_page_table->pointers[page_begin_off]
                = mem_ptr(
                    static_cast<uint8_t *>(mmap(nullptr, count * page_size, PROT_READ,
                        MAP_ANONYMOUS | MAP_PRIVATE, 0, 0)),
                    _free_mem);
#else
            current_page_table->pointers[page_begin_off]
                = mem_ptr(
                    reinterpret_cast<uint8_t *>(VirtualAlloc(nullptr, count * page_size,
                        MEM_RESERVE, PAGE_NOACCESS)),
                    _free_mem);
#endif
            for (size_t i = page_begin_off + 1; i < page_end_off; i++) {
                current_page_table->pointers[i] = mem_ptr(current_page_table->pointers[i - 1].get() + page_size);
            }

            cpu->map_backing_mem(addr, max_grow, current_page_table->pointers[page_begin_off].get(), cprot);
        }

        commit(addr + bottom, top - bottom);

        return ptr<void>(addr);
    }

    ptr<void> memory_system::chunk_range(size_t beg_addr, size_t end_addr, size_t bottom, size_t top, size_t max_grow, prot cprot) {
        // Find the reversed memory with this max grow
        size_t page_count = (max_grow + page_size - 1) / page_size;

        size_t page_begin_off = (beg_addr + page_size - 1) / page_size;
        size_t page_end_off = (end_addr - page_size + 1) / page_size;

        decltype(global_pages)::reverse_iterator page_begin;
        decltype(page_begin) page_end;

        size_t len = common::GB(4);

        if (beg_addr >= global_data && beg_addr < ram_drive) {
            page_begin = global_pages.rbegin() + ((ram_drive / page_size) - page_end_off);
            page_end = global_pages.rbegin() + ((ram_drive / page_size) - page_begin_off);
        } else if (beg_addr >= codeseg_addr && beg_addr < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.rbegin() + (((codeseg_addr + 0x10000000) / page_size) - page_end_off);
            page_end = codeseg_pages.rbegin() + (((codeseg_addr + 0x10000000) / page_size) - page_begin_off);
        } else {
            page_begin = current_page_table->pages.rbegin() + ((len / page_size) - page_end_off);
            page_end = current_page_table->pages.rbegin() + ((len / page_size) - page_begin_off);
        }

        page holder;

        auto &suitable_pages = std::search_n(page_begin, page_end, page_count, holder,
            [](const auto &lhs, const auto &rhs) {
                return (lhs.sts == page_status::free) && (lhs.generation == 0);
            });

        uint32_t idx;

        if (beg_addr >= global_data && beg_addr < ram_drive) {
            idx = global_pages.rend() - suitable_pages - page_count;

            if (suitable_pages != global_pages.rend()) {
                return chunk(global_data + idx * page_size, bottom, top, max_grow, cprot);
            }
        } else if (beg_addr >= ram_code_addr && beg_addr < codeseg_addr + 0x10000000) {
            idx = codeseg_pages.rend() - suitable_pages - page_count;

            if (suitable_pages != codeseg_pages.rend()) {
                return chunk(ram_code_addr + idx * page_size, bottom, top, max_grow, cprot);
            }
        }

        idx = current_page_table->pages.rend() - suitable_pages - page_count;

        if (suitable_pages != current_page_table->pages.rend()) {
            return chunk(idx * page_size, bottom, top, max_grow, cprot);
        }

        return ptr<void>(nullptr);
    }

    // Change the prot of pages
    int memory_system::change_prot(ptr<void> addr, size_t size, prot nprot) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        decltype(global_pages)::iterator page_begin;
        decltype(page_begin) page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
            page_begin = global_pages.begin() + (beg - (global_data / page_size));
            page_end = global_pages.begin() + (end - (global_data / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.begin() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.begin() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.begin() + beg;
            page_end = current_page_table->pages.begin() + end;
        }

        for (page_begin; page_begin != page_end; page_begin++) {
            // Only a commited region can have a protection
            if (page_begin->sts != page_status::committed) {
                return -1;
            }

            page_begin->page_protection = nprot;
        }

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
#ifdef WIN32
            DWORD oldprot = 0;
            auto res = VirtualProtect(global_pointers[(beg - (global_data / page_size)) * page_size].get(),
                count * page_size, translate_protection(nprot), &oldprot);

            if (!res) {
                return -1;
            }
#else
            mprotect(global_pointers[(beg - (global_data / page_size)) * page_size].get(),
                count * page_size, translate_protection(nprot));
#endif

            if (current_page_table) {
                std::copy(global_pages.begin() + beg - (global_data / page_size),
                    global_pages.begin() + end - (global_data / page_size), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + beg - (global_data / page_size),
                    global_pointers.begin() + end - (global_data / page_size), current_page_table->pointers.begin() + beg);
            }

            cpu->unmap_memory(addr.ptr_address(), size);
            cpu->map_backing_mem(addr.ptr_address(), size, global_pointers[beg - (global_data / page_size)].get(), nprot);
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
#ifdef WIN32
            DWORD oldprot = 0;
            auto res = VirtualProtect(codeseg_pointers[(beg - (ram_code_addr / page_size)) * page_size].get(),
                count * page_size, translate_protection(nprot), &oldprot);

            if (!res) {
                return -1;
            }
#else
            mprotect(codeseg_pointers[(beg - (ram_code_addr / page_size)) * page_size].get(),
                count * page_size, translate_protection(nprot));
#endif
            if (current_page_table) {
                std::copy(codeseg_pages.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pages.begin() + end - (codeseg_addr / page_size), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pointers.begin() + end - (codeseg_addr / page_size), current_page_table->pointers.begin() + beg);
            }

            cpu->unmap_memory(addr.ptr_address(), size);
            cpu->map_backing_mem(addr.ptr_address(), size, codeseg_pointers[beg - (codeseg_addr / page_size)].get(), nprot);
        } else {
#ifdef WIN32
            DWORD oldprot = 0;
            auto res = VirtualProtect(current_page_table->pointers[beg].get(),
                count * page_size, translate_protection(nprot), &oldprot);

            if (!res) {
                return -1;
            }
#else
            mprotect(current_page_table->pointers[beg].get(),
                count * page_size, translate_protection(nprot));
#endif

            cpu->unmap_memory(addr.ptr_address(), size);
            cpu->map_backing_mem(addr.ptr_address(), size, current_page_table->pointers[beg].get(), nprot);
        }

        return 0;
    }

    // Mark a chunk at addr as unusable
    int memory_system::unchunk(ptr<void> addr, size_t length) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + length - 1 + page_size) / page_size;

        size_t count = end - beg;

        decltype(global_pages)::iterator page_begin;
        decltype(page_begin) page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
            page_begin = global_pages.begin() + (beg - (global_data / page_size));
            page_end = global_pages.begin() + (end - (global_data / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.begin() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.begin() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.begin() + beg;
            page_end = current_page_table->pages.begin() + end;
        }

        for (page_begin; page_begin != page_end; page_begin++) {
            page_begin->sts = page_status::free;
            page_begin->page_protection = prot::none;
            page_begin->generation = 0;
        }

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
            global_pointers[(beg - (global_data / page_size)) * page_size].reset();

            if (current_page_table) {
                std::copy(global_pages.begin() + beg - (global_data / page_size),
                    global_pages.begin() + end - (global_data / page_size), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + beg - (global_data / page_size),
                    global_pointers.begin() + end - (global_data / page_size), current_page_table->pointers.begin() + beg);
            }
        } else if (addr.ptr_address() >= ram_code_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            codeseg_pointers[(beg - (ram_code_addr / page_size)) * page_size].reset();

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pages.begin() + end - (codeseg_addr / page_size), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pointers.begin() + end - (codeseg_addr / page_size), current_page_table->pointers.begin() + beg);
            }
        } else {
            current_page_table->pointers[beg].reset();
        }

        cpu->unmap_memory(addr.ptr_address(), count * page_size);

        return 0;
    }

    // Commit to page
    int memory_system::commit(ptr<void> addr, size_t size) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        decltype(global_pages)::iterator page_begin;
        decltype(page_begin) page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
            page_begin = global_pages.begin() + (beg - (global_data / page_size));
            page_end = global_pages.begin() + (end - (global_data / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.begin() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.begin() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.begin() + beg;
            page_end = current_page_table->pages.begin() + end;
        }

        prot nprot = page_begin->page_protection;

        for (page_begin; page_begin != page_end; page_begin++) {
            // Can commit on commited region or reserved region
            if (page_begin->sts == page_status::free) {
                return -1;
            }

            page_begin->sts = page_status::committed;
        }

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
#ifdef WIN32
            DWORD oldprot = 0;
            auto res = VirtualAlloc(global_pointers[beg - (global_data / page_size)].get(),
                count * page_size, MEM_COMMIT, translate_protection(nprot));

            if (!res) {
                return -1;
            }
#else
            mprotect(global_pointers[beg - (global_data / page_size)].get(),
                count * page_size, translate_protection(nprot));
#endif

            if (current_page_table) {
                std::copy(global_pages.begin() + beg - (global_data / page_size),
                    global_pages.begin() + end - (global_data / page_size), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + beg - (global_data / page_size),
                    global_pointers.begin() + end - (global_data / page_size), current_page_table->pointers.begin() + beg);
            }
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
#ifdef WIN32
            DWORD oldprot = 0;
            auto res = VirtualAlloc(codeseg_pointers[beg - (codeseg_addr / page_size)].get(),
                count * page_size, MEM_COMMIT, translate_protection(nprot));

            if (!res) {
                return -1;
            }
#else
            mprotect(codeseg_pointers[beg - (ram_code_addr / page_size)].get(),
                count * page_size, translate_protection(nprot));
#endif

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pages.begin() + end - (codeseg_addr / page_size), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pointers.begin() + end - (codeseg_addr / page_size), current_page_table->pointers.begin() + beg);
            }
        } else {
#ifdef WIN32
            DWORD oldprot = 0;
            auto res = VirtualAlloc(current_page_table->pointers[beg].get(),
                count * page_size, MEM_COMMIT, translate_protection(nprot));

            if (!res) {
                return -1;
            }
#else
            mprotect(current_page_table->pointers[beg].get(),
                count * page_size, translate_protection(nprot));
#endif
        }

        return 0;
    }

    // Decommit
    int memory_system::decommit(ptr<void> addr, size_t size) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        decltype(global_pages)::iterator page_begin;
        decltype(page_begin) page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
            page_begin = global_pages.begin() + (beg - (global_data / page_size));
            page_end = global_pages.begin() + (end - (global_data / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.begin() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.begin() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.begin() + beg;
            page_end = current_page_table->pages.begin() + end;
        }

        prot nprot = page_begin->page_protection;

        for (page_begin; page_begin != page_end; page_begin++) {
            if (page_begin->sts == page_status::committed) {
                page_begin->sts = page_status::reserved;
            }
        }

        if (addr.ptr_address() >= global_data && addr.ptr_address() < ram_drive) {
#ifdef WIN32
            auto res = VirtualFree(global_pointers[(beg - (global_data / page_size)) * page_size].get(),
                count * page_size, MEM_DECOMMIT);

            if (!res) {
                return -1;
            }
#else
            mprotect(global_pointers[(beg - (global_data / page_size)) * page_size].get(),
                count * page_size, PROT_NONE);
#endif

            if (current_page_table) {
                std::copy(global_pages.begin() + beg - (global_data / page_size),
                    global_pages.begin() + end - (global_data / page_size), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + beg - (global_data / page_size),
                    global_pointers.begin() + end - (global_data / page_size), current_page_table->pointers.begin() + beg);
            }
        } else if (addr.ptr_address() >= ram_code_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
#ifdef WIN32
            auto res = VirtualFree(codeseg_pointers[(beg - (ram_code_addr / page_size)) * page_size].get(),
                count * page_size, MEM_DECOMMIT);

            if (!res) {
                return -1;
            }
#else
            mprotect(codeseg_pointers[(beg - (ram_code_addr / page_size)) * page_size].get(),
                count * page_size, PROT_NONE);
#endif

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pages.begin() + end - (codeseg_addr / page_size), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pointers.begin() + end - (codeseg_addr / page_size), current_page_table->pointers.begin() + beg);
            }
        } else {
#ifdef WIN32
            auto res = VirtualFree(current_page_table->pointers[beg].get(),
                count * page_size, MEM_DECOMMIT);

            if (!res) {
                return -1;
            }
#else
            mprotect(current_page_table->pointers[beg].get(),
                count * page_size, PROT_NONE);
#endif
        }

        return 0;
    }

    void memory_system::read(address addr, void *data, size_t size) {
        void *fptr = get_real_pointer(addr);

        if (fptr == nullptr) {
            LOG_WARN("Reading invalid address: 0x{:x}", addr);
            return;
        }

        memcpy(data, fptr, size);
    }

    void memory_system::write(address addr, void *data, size_t size) {
        void *to = get_real_pointer(addr);

        if (to == nullptr) {
            LOG_WARN("Reading invalid address: 0x{:x}", addr);
            return;
        }

        memcpy(to, data, size);
    }

    page_table *memory_system::get_current_page_table() const {
        return current_page_table;
    }

    void memory_system::set_current_page_table(page_table &table) {
        current_page_table = &table;

        std::copy(codeseg_pages.begin(), codeseg_pages.end(), current_page_table->pages.begin() + (codeseg_addr / page_size));
        std::copy(codeseg_pointers.begin(), codeseg_pointers.end(), current_page_table->pointers.begin() + (codeseg_addr / page_size));

        std::copy(global_pages.begin(), global_pages.end(), current_page_table->pages.begin() + (global_data / page_size));
        std::copy(global_pointers.begin(), global_pointers.end(), current_page_table->pointers.begin() + (global_data / page_size));

        current_page_table->pointers[rom_addr / page_size] = mem_ptr(reinterpret_cast<uint8_t*>(rom_map), [](uint8_t*){});

        for (size_t i = 1; i < common::align(common::MB(41), page_size) / page_size; i++) {
            current_page_table->pointers[rom_addr / page_size + i] =
                mem_ptr(current_page_table->pointers[rom_addr / page_size + i - 1].get() + page_size, [](uint8_t*){});
        }

        cpu->page_table_changed();
    }
}