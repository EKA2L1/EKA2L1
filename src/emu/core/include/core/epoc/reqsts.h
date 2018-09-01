#pragma once

/* Header only request status */
namespace eka2l1::epoc {
    struct request_status {
        int status;
        int flags;

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
}