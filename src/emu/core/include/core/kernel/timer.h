#pragma once

#include <kernel/wait_obj.h>
#include <core_timing.h>

namespace eka2l1 {
	namespace kernel {
		enum class reset_type {
			oneshot,
			again
		};
		
		class timer : public wait_obj {
			std::string tname;
			timing_system* timing;

			reset_type rt;
			int callback_type;

		public:
			timer(timing_system* timing, std::string name, reset_type rt);
			~timer();

			bool signaled;

			uint64_t init_delay;
			uint64_t interval_delay;

			bool should_wait(const kernel::uid thr_id) override;
			bool acquire(const kernel::uid thr_id) override;

			void wake_up_waiting_threads() override;

			void set(int64_t init, int64_t interval);

			void cancel();
			void clear();

			void signal(int cycles_late);
		};
	}
}