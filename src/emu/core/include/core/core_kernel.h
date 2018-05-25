#pragma once

#include <kernel/kernel_obj.h>
#include <kernel/scheduler.h>
#include <arm/jit_interface.h>
#include <process.h>
#include <ptr.h>

#include <atomic>
#include <memory>
#include <map>
#include <mutex>

namespace eka2l1 {
    class timing_system;
	class memory_system;
	class manager_system;

    namespace kernel {
        class thread;

        using uid = uint64_t;
    }

    using thread_ptr = kernel::thread*;
	using process_ptr = std::shared_ptr<process>;

    class kernel_system {
        std::atomic<kernel::uid> crr_uid;
        std::map<kernel::uid, thread_ptr> threads;

        std::mutex mut;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;
		
		std::map<uint32_t, process_ptr> processes;

		timing_system* timing;
		manager_system* mngr;
		memory_system* mem;
		hle::lib_manager* libmngr;

    public:
        void init(timing_system* sys, manager_system* mngrsys,
			memory_system* mem_sys, hle::lib_manager* lib_sys, arm::jit_interface* cpu);
        void shutdown();

		void reschedule() {
			thr_sch->reschedule();
		}

		void unschedule_wakeup() {
			thr_sch->unschedule_wakeup();
		}

		void unschedule(kernel::uid thread_id) {
			thr_sch->unschedule(thread_id);
		}

        kernel::uid next_uid();

        void add_thread(kernel::thread *thr);
        bool run_thread(kernel::uid thr);

		process* spawn_new_process(std::string& path, std::string name, uint32_t uid);
		process* spawn_new_process(uint32_t uid);

        kernel::thread *crr_thread();
    };
}
