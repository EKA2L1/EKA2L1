#pragma once

#include <string>

namespace eka2l1 {
	// EABI emulation
	namespace eabi {
		// Info Helper typeinfo: vtable and name. No
		// other helper class required.
		class info_helper {
			std::string descriptor;

		public:
			void info_helper_construct();
			void info_helper_destruct();

			bool is_pointer() const;
			bool is_function() const;

			void do_catch(info_helper* helper, void** ptr, unsigned int wat) const;
			void do_upcast(info_helper* helper, void** ptr) const;
			void do_upcast(info_helper* helper, void** ptr, int res) const;
		};

		std::string demangle(std::string target);
		std::string mangle(std::string target);
	}
}