#include <epoc/ptr.h>
#include <epoc/kernel/process.h>

namespace eka2l1 {
    void *get_raw_pointer(process_ptr pr, address addr) {
        return pr->get_ptr_on_addr_space(addr);
    }
}