#pragma once

#include <kernel/kernel_obj.h>
#include <kernel/thread.h>

#include <common/linked.h>
#include <set>

namespace eka2l1::kernel {
    class mutex;

    class condvar : public kernel_obj {
    private:
        kernel::thread_priority_queue waits;
        common::roundabout suspended;

        mutex *holding_;
        ntimer *timing_;

        int timeout_event_;
        std::set<thread*> timing_out_thrs_;

        void unblock_thread(thread *thr);

    public:
        explicit condvar(kernel_system *kern, ntimer *timing, kernel::process *owner,
            std::string name, kernel::access_type access = kernel::access_type::local_access);
        ~condvar() override;

        bool wait(thread *thr, mutex *mut, const int timeout);
        void signal();
        void broadcast();

        bool suspend_thread(thread *thr);
        bool unsuspend_thread(thread *thr);
        void priority_change();

        void on_timeout(std::uint64_t userdata, int late);
    };
}