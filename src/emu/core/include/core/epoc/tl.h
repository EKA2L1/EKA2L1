#pragma once

#include <kernel/thread.h>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    eka2l1::kernel::thread_local_data &current_local_data(eka2l1::system *sys);
    eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr);
}