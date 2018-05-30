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
    void _free_mem(uint8_t *dt) {
#ifndef WIN32
        munmap(dt, common::GB(1));
#else
        VirtualFree(dt, 0, MEM_RELEASE);
#endif
    }

    void memory_system::shutdown() {
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
		}
		else {
			LOG_INFO("Virtual memory allocated: 0x{:x}", (size_t)memory.get());
		}

		allocated_pages.resize(len / page_size);
    }

    void memory_system::alloc_inner(address addr, size_t pg_count, allocated::iterator blck) {
        uint8_t *addr_mem = &memory[addr];
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
    address memory_system::alloc(size_t size) {
        const size_t page_count = (size + (page_size - 1)) / page_size;

        const size_t page_heap_start = (LOCAL_DATA / page_size) + 1;
        const size_t page_heap_end = (DLL_STATIC_DATA / page_size) - 1;

        const auto start_heap_page = allocated_pages.begin() + page_heap_start;
        const auto end_heap_page = allocated_pages.begin() + page_heap_end;

        const auto &free_block = std::search_n(start_heap_page, end_heap_page, page_count, 0);

        if (free_block != allocated_pages.end()) {
            const size_t block_page_index = free_block - allocated_pages.begin();
            const address addr = static_cast<address>(block_page_index * page_size);

            alloc_inner(addr, page_count, free_block);

            return addr;
        }

        return 0;
    }

    void memory_system::free(address addr) {
        const size_t page = addr / page_size;
        const gen generation = allocated_pages[page];

        const auto end_heap_page = allocated_pages.end();

        const auto different_gen = std::bind(std::not_equal_to<gen>(), generation, std::placeholders::_1);
        const auto &first_page = allocated_pages.begin() + page;
        const auto &last_page = std::find_if(first_page, end_heap_page, different_gen);
        std::fill(first_page, last_page, 0);
    }

    // Map dynamicly still fine. As soon as user call IME_RANGE,
    // that will call the UC and execute it
    // Returns a pointer that is aligned and mapped
    ptr<void> memory_system::map(address addr, size_t size, prot cprot) {
        if (addr <= NULL_TRAP && addr != 0) {
            LOG_INFO("Unmapable region 0x{:x}", addr);
            return ptr<void>();
        }

        address page_addr = (addr / page_size);
        page_addr = page_addr * page_size;

        void *real_address = &memory[page_addr];
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

    int memory_system::change_prot(address addr, size_t size, prot nprot) {
        auto tprot = translate_protection(nprot);
        void *real_addr = get_addr<void>(addr);

#ifdef WIN32
        DWORD old_prot = 0;
        return VirtualProtect(real_addr, size, tprot, &old_prot);
#else
        return mprotect(real_addr, size, tprot);
#endif
    }

    int memory_system::unmap(ptr<void> addr, size_t size) {
#ifndef WIN32
        return munmap(addr.get(this), size);
#else
        return VirtualFree(addr.get(this), size, MEM_DECOMMIT);
#endif
    }

    // Alloc from thread heap
    address memory_system::alloc_range(address beg, address end, size_t size) {
        const size_t page_count = (size + (page_size - 1)) / page_size;

        const size_t page_heap_start = (beg / page_size) + 1;
        const size_t page_heap_end = (end / page_size) - 1;

        const auto start_heap_page = allocated_pages.begin() + page_heap_start;
        const auto end_heap_page = allocated_pages.begin() + page_heap_end;

        const auto &free_block = std::search_n(start_heap_page, end_heap_page, page_count, 0);

        if (free_block != allocated_pages.end()) {
            const size_t block_page_index = free_block - allocated_pages.begin();
            const address addr = static_cast<address>(block_page_index * page_size);

            alloc_inner(addr, page_count, free_block);

            return addr;
        }

        return 0;
    }

    // Load the ROM into virtual memory, using ma
    bool memory_system::load_rom(const std::string& rom_path) {
		FILE* f = fopen(rom_path.c_str(), "rb");

		if (f == nullptr) {
			return false;
		}

		fseek(f, 0, SEEK_END);

		auto size = ftell(f);
		auto aligned_size = ((size / page_size) + 1) * (page_size);
		auto left = size;

		fseek(f, 0, SEEK_SET);

#ifdef WIN32
		DWORD old_prot;
		auto newptr = VirtualAlloc(ptr<void>(0x80000000).get(this), aligned_size, MEM_COMMIT, PAGE_READWRITE);

		if (!newptr) {
			return false;
		}
#else
		mprotect(ptr<void>(0x80000000).get(this), left, PROT_READ | PROT_WRITE);
#endif

		long buf_once = 0;

		ptr<void> start(0x80000000);

		while (left) {
			buf_once = common::min(left, (long)100000);
			fread(start.get(this), 1, buf_once, f);
			start += (address)buf_once;

			left -= buf_once;
		}

		fclose(f);

#ifdef WIN32
		bool res = VirtualProtect(ptr<void>(0x80000000).get(this),aligned_size, PAGE_READONLY, &old_prot);

		if (!res) {
			LOG_WARN("Can't change protection of ROM memory back to read-only.");
		}
#else
		mprotect(ptr<void>(0x80000000).get(this), size, PROT_READ);
#endif
		return true;
    }

    address memory_system::alloc_ime(size_t size) {
        address addr = alloc_range(RAM_CODE_ADDR, ROM, size);
        int res = change_prot(addr, size, prot::read_write_exec);
        return addr;
    }
}
