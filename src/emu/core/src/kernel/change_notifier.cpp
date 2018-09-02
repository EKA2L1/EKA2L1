#include <core/core_kernel.h>
#include <core/kernel/change_notifier.h>

#include <common/cvt.h>
#include <common/random.h>

namespace eka2l1 {
    namespace kernel {
        change_notifier::change_notifier(kernel_system *kern)
            : eka2l1::kernel::kernel_obj(kern, "changenotifier" + common::to_string(eka2l1::random()),
                  kernel::access_type::local_access) {
            obj_type = object_type::change_notifier;
        }

        bool change_notifier::logon(epoc::request_status *request_sts) {
            if (request_status) {
                return false;
            }

            requester = kern->crr_thread();
            request_status = request_sts;

            return true;
        }

        bool change_notifier::logon_cancel() {
            if (!request_status) {
                return false;
            }

            *request_status = -3;
            requester->signal_request();

            requester = nullptr;
            request_status = nullptr;

            return true;
        }

        void change_notifier::notify_change_requester() {
            *request_status = 0;
            requester->signal_request();

            requester = nullptr;
            request_status = nullptr;
        }
    }
}