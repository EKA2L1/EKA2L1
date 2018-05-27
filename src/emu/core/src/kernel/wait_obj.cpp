#include <kernel/wait_obj.h>
#include <kernel/thread.h>
#include <algorithm>

namespace eka2l1 {
	namespace kernel {
		// Based on Citra

		void wait_obj::add_waiting_thread(thread_ptr thr) {
			auto res = std::find_if(waits.begin(), waits.end(),
				[thr](auto& thr_ptr) { return thr_ptr->obj_id == thr->obj_id; });
		
			if (res != waits.end()) {
				waits.push_back(thr);
			}
		}

		void wait_obj::erase_waiting_thread(kernel::uid thr) {
			auto res = std::find_if(waits.begin(), waits.end(),
				[thr](auto& thr_ptr) { return thr_ptr->obj_id == thr; });

			if (res != waits.end()) {
				waits.erase(res);
			}
		}

		thread_ptr wait_obj::next_ready_thread() {
			thread* next = nullptr;
			uint32_t next_pri = kernel::priority_absolute_background + 1;

			for (const auto& wait_thread : waits) {
				if (next->current_priority() >= next_pri) {
					continue;
				}

				if (should_wait(wait_thread->unique_id())) {
					continue;
				}

				bool ready_to_run = true;

				if (wait_thread->current_state() == thread_state::wait_fast_sema) {
					ready_to_run = std::none_of(wait_thread->waits.begin(), wait_thread->waits.end(),
						[wait_thread](auto& obj) { return obj->should_wait(wait_thread->unique_id()); });
				}

				if (ready_to_run) {
					next = &(*wait_thread);
					next_pri = wait_thread->current_priority();
				}
 			}

			return thread_ptr(next);
		}

		void wait_obj::wake_up_waiting_threads() {
			while (auto thr = next_ready_thread()) {
				for (auto& obj : thr->waits) {
					obj->acquire(thr->obj_id);
				}

				for (auto& obj : thr->waits) {
					obj->erase_waiting_thread(thr->obj_id);
				}
				
				thr->waits.clear();
				thr->resume();
			}
		}
	}
}