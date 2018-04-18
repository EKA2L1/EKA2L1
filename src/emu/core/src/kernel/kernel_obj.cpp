#include <kernel/kernel_obj.h>
#include <core_kernel.h>

namespace eka2l1 {
    namespace kernel {
        kernel_obj::kernel_obj(const std::string& obj_name)
            : obj_name(obj_name) {
            obj_id = core_kernel::next_uid();
        }
    }
}
