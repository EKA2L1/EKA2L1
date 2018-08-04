#pragma once

#include <core/kernel/kernel_obj.h>

#include <memory>

namespace eka2l1 {
    class kernel_system;
    
    namespace kernel {
        class thread;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
}

namespace eka2l1 {
    namespace kernel {
        class change_notifier : public eka2l1::kernel::kernel_obj {
            int *request_status;
            thread_ptr requester;

        public:
            change_notifier(kernel_system *kern);

            bool logon(int *request_sts);
            bool logon_cancel();

            void notify_change_requester();
        };
    }
}