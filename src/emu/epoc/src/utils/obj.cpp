#include <epoc/utils/obj.h>

namespace eka2l1::epoc {
    object_table::object_table(): 
        next_instance(1) {
    }

    static handle make_handle(const std::uint32_t idx, const std::uint32_t inst) {
        return static_cast<handle>((inst << 16) | (idx));
    }
        
    handle object_table::add(ref_count_object_ptr obj) {
        for (std::size_t i = 0; i < objects.size(); i++) {
            if (!objects[i]) {
                objects[i] = std::move(obj);
                objects[i]->id = next_instance;

                return make_handle(static_cast<std::uint32_t>(i + 1), next_instance++);
            }
        }

        objects.push_back(std::move(obj));
        objects.back()->id = next_instance;
        
        return make_handle(static_cast<std::uint32_t>(objects.size()), next_instance++);
    }

    bool   object_table::remove(handle obj_handle) {
        const std::size_t idx = obj_handle & 0xFFFF;
        if (idx > objects.size() || idx == 0) {
            return false;
        }

        objects[idx - 1].reset();
        return true;
    }

    ref_count_object_ptr object_table::get_raw(handle obj_handle) {
        const std::size_t idx = obj_handle & 0xFFFF;
        if (idx > objects.size() || idx == 0) {
            return false;
        }
        
        return objects[idx - 1];
    }
}
