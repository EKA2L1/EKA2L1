#pragma once

#include <kernel/kernel_obj.h>
#include <memory>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class thread;
        using thread_ptr = std::shared_ptr<thread>;

        class wait_obj : public kernel_obj {
        public:
            wait_obj(kernel_system *sys, std::string name)
                : kernel_obj(sys, name) {
                waits.clear();
            }

            std::vector<thread_ptr> waits;

            virtual bool should_wait(const kernel::uid thr) = 0;
            virtual void acquire(const kernel::uid thr) = 0;

            virtual void add_waiting_thread(thread_ptr thr);
            virtual void erase_waiting_thread(kernel::uid thr);

            virtual void wake_up_waiting_threads();
            thread_ptr next_ready_thread();

            const std::vector<thread_ptr> &waiting_threads() const {
                return waits;
            }
        };
    }
}