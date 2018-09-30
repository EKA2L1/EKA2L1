#pragma once

#include <core/kernel/kernel_obj.h>
#include <core/epoc/reqsts.h>
#include <core/ptr.h>

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

            void write_object_to_snapshot(common::wo_buf_stream &stream) override;
            void do_state(common::ro_buf_stream &stream) override;
        };
    }
}