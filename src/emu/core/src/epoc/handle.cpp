#include <epoc/handle.h>

namespace eka2l1::epoc {
    eka2l1::kernel_obj_ptr RHandleBase::GetKObject(eka2l1::system *sys) {
        return sys->get_kernel_system()->get_kernel_obj(iHandle);
    }

    eka2l1::process_ptr RProcess::GetProcess(eka2l1::system *sys) {
        return sys->get_kernel_system()->get_process(iAppId);
    }
}