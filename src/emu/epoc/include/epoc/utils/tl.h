#pragma once

#include <epoc/kernel/thread.h>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    /*! \brief Get the current thread local data. 
     * \param sys The system.
     * \returns Reference to the local data.
     */
    eka2l1::kernel::thread_local_data &current_local_data(eka2l1::system *sys);
    
    /*! \brief Get the tls slot based on the handle (address). 
     * \param sys The system.
     * \param addr The DLL address (handle).
     * \returns Pointer to the tls slot.
     */
    eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr);
}