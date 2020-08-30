#pragma once

#include <mem/ptr.h>
#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    namespace common {
        class chunkyseri;
    }

    using thread_ptr = kernel::thread *;
}

/* Header only request status */
namespace eka2l1::epoc {
    // Don't change the structure! Specifically no more fields and no vtable!
    struct request_status {
        int status;
        int flags;

        void do_state(common::chunkyseri &seri);

        enum flag_type {
            active = 1,
            pending = 2
        };

        void set(const int sts, const bool is_eka1) {
            if (!is_eka1) {
                if (sts == 0x80000001) {
                    flags |= pending;
                } else {
                    flags &= ~pending;
                }
            }

            status = sts;
        }

        request_status(const int sts, const bool is_eka1)
            : flags(0) {
            set(sts, is_eka1);
        }
    };

    struct notify_info {
        eka2l1::ptr<epoc::request_status> sts = 0;
        eka2l1::kernel::thread *requester;
        bool is_eka1;

        explicit notify_info() = default;

        explicit notify_info(eka2l1::ptr<epoc::request_status> &sts, eka2l1::kernel::thread *requester)
            : sts(sts)
            , requester(requester) {
        }

        void complete(int err_code);
        void do_state(common::chunkyseri &seri);

        bool empty() const {
            return !sts;
        }

        const bool operator==(const notify_info &rhs) const {
            return (sts == rhs.sts) && (requester == rhs.requester);
        }
    };
}
