#pragma once

#include <kernel/kernel_obj.h>
#include <kernel/scheduler.h>
#include <arm/jit_interface.h>

#include <atomic>
#include <memory>
#include <map>
#include <mutex>

namespace eka2l1 {
    class timing_system;

    namespace kernel {
        class thread;

        using uid = uint64_t;
    }

    using thread_ptr = kernel::thread*;

    class kernel_system {
        std::atomic<kernel::uid> crr_uid;
        std::map<kernel::uid, thread_ptr> threads;

        std::mutex mut;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;

		timing_system* timing;

    public:
        void init(timing_system* sys, arm::jit_interface* cpu);
        void shutdown();

		void reschedule() {
			thr_sch->reschedule();
		}

        kernel::uid next_uid();

        void add_thread(kernel::thread *thr);
        bool run_thread(kernel::uid thr);

        kernel::thread *crr_thread();
    };
}
