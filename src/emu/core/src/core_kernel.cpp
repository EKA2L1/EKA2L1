#include <algorithm>
#include <atomic>
#include <queue>
#include <thread>

#include <core_kernel.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <ptr.h>

namespace eka2l1 {
    void kernel_system::init() {
        // Intialize the uid with zero
        crr_uid.store(0);
        thr_sch = std::make_shared<kernel::thread_scheduler>(100);
    }

    void kernel_system::shutdown() {
        thr_sch.reset();
        crr_uid.store(0);
    }

    kernel::uid kernel_system::next_uid() {
        crr_uid++;
        return crr_uid.load();
    }

    void kernel_system::add_thread(kernel::thread *thr) {
        const std::lock_guard<std::mutex> guard(mut);
        threads.emplace(thr->unique_id(), thr);
    }

    bool kernel_system::run_thread(kernel::uid thr_id) {
        auto res = threads.find(thr_id);

        if (res == threads.end()) {
            return false;
        }

        thr_sch->schedule(res->second.get());

        return true;
    }

    kernel::thread *kernel_system::crr_running_thread() {
        return thr_sch->current_running_thread();
    }
}
