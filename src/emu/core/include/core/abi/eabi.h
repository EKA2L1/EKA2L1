#pragma once

#include <string>
#include <typeinfo>

namespace eka2l1 {
	// EABI emulation
	namespace eabi {
		std::string demangle(std::string target);
		std::string mangle(std::string target);

		void pure_virtual_call();
		void deleted_virtual_call(); 

        void leave(uint32_t id, const std::string& msg);
		void trap_leave(uint32_t id);
	}
}