#include <atomic>
#include <thread>
#include <queue>

#include <core_kernel.h>
#include <kernel/thread.h>

namespace eka2l1 {
    namespace core_kernel {
        std::atomic<kernel::uid> crr_uid;
        std::priority_queue<kernel::thread_ptr> threads;

        std::mutex mut;

        void init() {
            // Intialize the uid with zero
            crr_uid.store(0);
        }

        kernel::uid next_uid() {
            crr_uid++;
            return crr_uid.load();
        }

        void add_thread(kernel::thread* thr) {
            const std::lock_guard<std::mutex> guard(mut);
            threads.emplace(thr);
        }
    }
}
