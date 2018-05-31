#include <core_kernel.h>
#include <kernel/kernel_obj.h>

namespace eka2l1 {
    namespace kernel {
        kernel_obj::kernel_obj(kernel_system *kern, const std::string &obj_name)
            : obj_name(obj_name)
            , kern(kern) {
            obj_id = kern->next_uid();
        }
    }
}
