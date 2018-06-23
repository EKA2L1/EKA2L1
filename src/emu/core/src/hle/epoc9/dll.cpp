#include <epoc9/dll.h>
#include <loader/eka2img.h>
#include <loader/romimage.h>

using namespace eka2l1;

eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr) {
    return sys->get_kernel_system()->crr_thread()->get_tls_slot(addr, addr);
}
