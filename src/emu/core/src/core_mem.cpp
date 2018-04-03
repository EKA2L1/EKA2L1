#include <common/algorithm.h>
#include <common/log.h>

#include <core/core_mem.h>

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

namespace eka2l1 {
    namespace core_mem {
        using gen = size_t;
        using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t*)>>;
        using allocated = std::vector<gen>;

        uint64_t  page_size;
        gen       generations;
        mem       memory;
        allocated allocated_pages;

        void _free_mem(uint8_t* dt) {
#ifndef WIN32
            munmap(dt, common::GB(1));
#else
            VirtualFree(dt, 0, MEM_RELEASE);
#endif
        }

        void init() {
#ifdef WIN32
            SYSTEM_INFO system_info = {};
            GetSystemInfo(&system_info);

            page_size = system_info.dwPageSize;
#else
            page_size = sysconf(_SC_PAGESIZE);
#endif

            uint32_t len = common::GB(1);

#ifndef WIN32
            memory = mem(static_cast<uint8_t*>
                            (mmap(nullptr, len, PROT_NONE,
                                  MAP_ANONYMOUS | MAP_PRIVATE,0, 0)), _free_mem);
#else
            memory = mem(static_cast<uint8_t*>
                            (VirtualAlloc(nullptr, len, MEM_REVERSE, PAGE_NOACCESS), _free_mem);
#endif

            if (!memory) {
                LOG_CRITICAL("Allocating virtual memory for emulating failed!");
                return;
            }

            allocated_pages.resize(len / page_size);
            alloc(page_size);

#ifdef WIN32
            DWORD old_protect = 0;
            const BOOL res = VirtualProtect(memory.get(), page_size, PAGE_NOACCESS, &old_protect);
#else
            mprotect(memory.get(), page_size, PROT_NONE);
#endif
        }

        void alloc_inner(address addr, size_t pg_count, allocated::iterator blck) {
            uint8_t* addr_mem = &memory[addr];
            auto aligned_size = pg_count * page_size;

            std::fill_n(blck, pg_count, generations);

#ifdef WIN32
            VirtualAlloc(addr_mem, aligned_size, MEM_COMMIT, PAGE_READWRITE);
#else
            mprotect(addr_mem, aligned_size, PROT_READ | PROT_WRITE);
#endif
            std::memset(addr_mem, 0, aligned_size);
        }

        address alloc(size_t size) {
            const size_t page_count = (size + (page_size - 1)) / page_size;
            const auto& free_block = std::search_n(allocated_pages.begin(), allocated_pages.end(), page_count, 0);

            if (free_block != allocated_pages.end()) {
                const size_t block_page_index = free_block -allocated_pages.begin();
                const address addr = static_cast<address>(block_page_index * page_size);

                alloc_inner(addr, page_count, free_block);

                return addr;
            }

            return 0;
        }

        void free(address addr) {
           const size_t page = addr / page_size;
           const gen generation = allocated_pages[page];

           const auto different_gen = std::bind(std::not_equal_to<gen>(), generation, std::placeholders::_1);
           const auto& first_page = allocated_pages.begin() + page;
           const auto& last_page = std::find_if(first_page, allocated_pages.end(), different_gen);
           std::fill(first_page, last_page, 0);
        }
    }
}
