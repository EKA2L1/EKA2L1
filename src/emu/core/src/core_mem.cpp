#include <common/algorithm.h>
#include <common/log.h>
#include <core/ptr.h>

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

// Why I make this advanced is because many emulators in Symbian
// and game use hack to run dynamic code (relocate code at runtime)
// The focus here is that mapping memory and allocate things is fine

namespace eka2l1 {
    namespace core_mem {
        using gen = size_t;
        using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t*)>>;
        using allocated = std::vector<gen>;

        enum {
            MAP_GEN_BASE = 0xF000
        };

        uint64_t  page_size;
        gen       generations;
        mem       memory;
        allocated allocated_pages;

        address   crr_heap;
        size_t    crr_heap_size;

        void _free_mem(uint8_t* dt) {
#ifndef WIN32
            munmap(dt, common::GB(1));
#else
            VirtualFree(dt, 0, MEM_RELEASE);
#endif
        }

        void shutdown() {
            memory.reset();
        }

        void init() {
#ifdef WIN32
            SYSTEM_INFO system_info = {};
            GetSystemInfo(&system_info);

            page_size = system_info.dwPageSize;
#else
            page_size = sysconf(_SC_PAGESIZE);
#endif

            size_t len = common::GB(4);

#ifndef WIN32
            memory = mem(static_cast<uint8_t*>
                            (mmap(nullptr, len, PROT_READ,
                                  MAP_ANONYMOUS | MAP_PRIVATE,0, 0)), _free_mem);
#else
            memory = mem(reinterpret_cast<uint8_t*>
                            (VirtualAlloc(nullptr, len, MEM_RESERVE, PAGE_READONLY), _free_mem));
#endif

            LOG_INFO("Virtual memory allocated: 0x{:x}", (size_t)memory.get());

            if (!memory) {
                LOG_CRITICAL("Allocating virtual memory for emulating failed!");
                return;
            }

            allocated_pages.resize(len / page_size);

#ifdef WIN32
            DWORD old_protect = 0;
            const BOOL res = VirtualProtect(memory.get(), page_size, PAGE_READONLY, &old_protect);
#else
            mprotect(memory.get(), page_size, PROT_READ);
#endif
        }

        void alloc_inner(address addr, size_t pg_count, allocated::iterator blck) {
            uint8_t* addr_mem = &memory[addr];
            auto aligned_size = pg_count * page_size;

            const gen generation = ++generations;
            std::fill_n(blck, pg_count, generation);

#ifdef WIN32
            VirtualAlloc(addr_mem, aligned_size, MEM_COMMIT, PAGE_READWRITE);
#else
            mprotect(addr_mem, aligned_size, PROT_READ | PROT_WRITE);
#endif
            std::memset(addr_mem, 0, aligned_size);
        }

        // Allocate the memory in heap memory
        address alloc(size_t size) {
            const size_t page_count = (size + (page_size - 1)) / page_size;

            const size_t page_heap_start = (LOCAL_DATA / page_size)+ 1;
            const size_t page_heap_end = (DLL_STATIC_DATA / page_size) - 1;

            const auto start_heap_page = allocated_pages.begin() + page_heap_start;
            const auto end_heap_page = allocated_pages.begin() + page_heap_end;

            const auto& free_block = std::search_n(start_heap_page, end_heap_page, page_count, 0);

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

           const size_t page_heap_end = (DLL_STATIC_DATA / page_size) - 1;
           const auto end_heap_page = allocated_pages.begin() + page_heap_end;

           const auto different_gen = std::bind(std::not_equal_to<gen>(), generation, std::placeholders::_1);
           const auto& first_page = allocated_pages.begin() + page;
           const auto& last_page = std::find_if(first_page, end_heap_page, different_gen);
           std::fill(first_page, last_page, 0);
        }

        // Map dynamicly still fine. As soon as user call IME_RANGE,
        // that will call the UC and execute it
        // Returns a pointer that is aligned and mapped
        ptr<void> map(address addr, size_t size, prot cprot) {
            if (addr <= NULL_TRAP && addr != 0) {
                LOG_INFO("Unmapable region 0x{:x}", addr);
                return ptr<void>();
            }

            address page_addr = (addr / page_size);
            page_addr = page_addr * page_size;

            void* real_address = &memory[page_addr];
            auto tprot = translate_protection(cprot);

            int res = 0;

#ifdef WIN32
            VirtualAlloc(real_address, size, MEM_COMMIT, tprot);
#else
            res = mprotect(real_address, size, tprot);

			if (res == -1) {
				LOG_ERROR("Can not map: 0x{:x}, size = {}", addr, size);
			}
#endif

            return ptr<void>(page_addr);
        }

        int      change_prot(address addr, size_t size, prot nprot) {
            auto tprot = translate_protection(nprot);
            void* real_addr = get_addr<void>(addr);

#ifdef WIN32
            DWORD old_prot = 0;
            return VirtualProtect(real_addr, size, tprot, &old_prot);
#else
            return mprotect(real_addr, size, tprot);
#endif
        }

        int  unmap(ptr<void> addr, size_t size) {
#ifndef WIN32
            return munmap(addr.get(), size);
#else
            return VirtualFree(addr.get(), size, MEM_DECOMMIT);
#endif
        }

        // Alloc from thread heap
        address heap_alloc(size_t size) {
            const size_t page_count = (size + (page_size - 1)) / page_size;

            const size_t page_heap_start = (crr_heap / page_size)+ 1;
            const size_t page_heap_end = (crr_heap + crr_heap_size / page_size) - 1;

            const auto start_heap_page = allocated_pages.begin() + page_heap_start;
            const auto end_heap_page = allocated_pages.begin() + page_heap_end;

            const auto& free_block = std::search_n(start_heap_page, end_heap_page, page_count, 0);

            if (free_block != allocated_pages.end()) {
                const size_t block_page_index = free_block -allocated_pages.begin();
                const address addr = static_cast<address>(block_page_index * page_size);

                alloc_inner(addr, page_count, free_block);

                return addr;
            }

            return 0;
        }

        // Set the current thread heap region, specif where heap
        // alloc must do allocation
        void set_crr_thread_heap_region(const address where, size_t size) {
            crr_heap = where;
            crr_heap_size = std::min((size_t)DLL_STATIC_DATA - where, size);
        }
    }

}
