#pragma once

#include <kernel/kernel_obj.h>
#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    namespace core_kernel {
        void init();
        void shutdown();

        kernel::uid next_uid();
        void add_thread(kernel::thread* thr);
    }
}
