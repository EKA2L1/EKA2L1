#include <atomic>
#include <algorithm>
#include <thread>
#include <queue>

#include <core_kernel.h>
#include <kernel/thread.h>
#include <kernel/scheduler.h>
#include <ptr.h>

namespace eka2l1 {
    namespace core_kernel {
        std::atomic<kernel::uid> crr_uid;
        std::map<kernel::uid, kernel::thread_ptr> threads;

        std::mutex mut;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;

        void init() {
            // Intialize the uid with zero
            crr_uid.store(0);
            thr_sch = std::make_shared<kernel::thread_scheduler>(100);
        }

        void shutdown() {
            thr_sch.reset();
            crr_uid.store(0);
        }

        kernel::uid next_uid() {
            crr_uid++;
            return crr_uid.load();
        }

        void add_thread(kernel::thread* thr) {
            const std::lock_guard<std::mutex> guard(mut);
            threads.emplace(thr->unique_id(), thr);
        }

        bool run_thread(kernel::uid thr_id) {
            auto res = threads.find(thr_id);

            if (res == threads.end()) {
                return false;
            }

            thr_sch->schedule(res->second.get());

            return true;
        }

        kernel::thread* crr_running_thread() {
            return thr_sch->current_running_thread();
        }
    }
}
