#pragma once

#include <kernel/kernel_obj.h>
#include <functional>

namespace eka2l1 {
	namespace kernel {
		using run_func = std::function<void()>;

		class active_obj {
			bool is_active;
			uint8_t priority;

			run_func run;

		public:
			bool active() const {
				return is_active;
			}

			void set_active() {
				is_active = true;
			}

			void set_active_func(run_func rf) {
				run = rf;
			}
		};
	}
}