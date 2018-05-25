#pragma once

#include "core_mem.h"
#include <cstdint>

namespace eka2l1 {
    // Symbian is 32 bit
    using address = uint32_t;

    template <typename T>
	class ptr {
		address mem_address;

	public:
		ptr()
			: mem_address(0) {}

		ptr(const T *ptr) {
			mem_address = (address)ptr;
		}

		ptr(const uint32_t addr)
			: mem_address(addr) {}

		address ptr_address() {
			return mem_address;
		}

		T *get(memory_system* mem) const {
			return mem->get_addr<T>(mem_address);
		}

		void reset() {
			mem_address = 0;
		}

		explicit operator bool() const {
			return mem_address != 0;
		}

		template <typename U>
		ptr<U> cast() const {
			return ptr<U>(mem_address);
		}

		ptr operator + (const ptr& rhs) {
			return ptr(mem_address + rhs.mem_address);
		}

		void operator += (const ptr& rhs) {
			mem_address += rhs.mem_address;
		}

		void operator += (const address rhs) {
			mem_address += rhs;
		}
    };
}
