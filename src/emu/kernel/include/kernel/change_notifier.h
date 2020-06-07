#pragma once

#include <kernel/kernel_obj.h>
#include <utils/reqsts.h>

#include <mem/mem.h>

#include <memory>

namespace eka2l1::kernel {
    class thread;

    class change_notifier : public eka2l1::kernel::kernel_obj {
        epoc::notify_info req_info_;

    public:
        explicit change_notifier(kernel_system *kern);

        bool logon(eka2l1::ptr<epoc::request_status> request_sts);
        bool logon_cancel();

        void notify_change_requester();

        void do_state(common::chunkyseri &seri) override;
    };
}
