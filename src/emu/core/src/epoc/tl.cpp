#include <core/epoc/tl.h>
#include <core/core.h>

namespace eka2l1::epoc {
    eka2l1::kernel::thread_local_data &current_local_data(eka2l1::system *sys) {
        return sys->get_kernel_system()->crr_thread()->get_local_data();
    }

    eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr) {
        return sys->get_kernel_system()->crr_thread()->get_tls_slot(addr, addr);
    }
}