#pragma once

#include <epoc/kernel/kernel_obj.h>
#include <epoc/utils/reqsts.h>

#include <epoc/mem.h>
#include <epoc/ptr.h>

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
            eka2l1::ptr<epoc::request_status> request_status;
            thread_ptr requester;

        public:
            change_notifier(kernel_system *kern);

            bool logon(eka2l1::ptr<epoc::request_status> request_sts);
            bool logon_cancel();

            void notify_change_requester();

            void do_state(common::chunkyseri &seri) override;
        };
    }
}