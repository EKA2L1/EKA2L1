#pragma once

#include <epoc/ptr.h>
#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }

    namespace common {
        class chunkyseri;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
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

        request_status(const int sts)
            : flags(0) {
            if (sts == 0x80000001) {
                flags |= pending;
            } else {
                flags &= ~pending;
            }

            status = sts;
        }

        void operator=(const int sts) {
            if (sts == 0x80000001) {
                flags |= pending;
            } else {
                flags &= ~pending;
            }

            status = sts;
        }
    };

    struct notify_info {
        eka2l1::ptr<epoc::request_status> sts = 0;
        eka2l1::kernel::thread *requester;

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

        const bool operator == (const notify_info &rhs) const {
            return (sts == rhs.sts) && (requester == rhs.requester);
        }
    };
}
