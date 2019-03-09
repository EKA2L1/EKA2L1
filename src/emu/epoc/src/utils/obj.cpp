#include <epoc/utils/obj.h>

namespace eka2l1::epoc {
    object_table::object_table(): 
        next_instance(1) {
    }

    static handle make_handle(const std::uint32_t idx, const std::uint32_t inst) {
        return static_cast<handle>((inst << 16) | (idx));
    }
        
    handle object_table::add(ref_count_object *obj) {
        for (std::size_t i = 0; i < objects.size(); i++) {
            if (!objects[i]) {
                objects[i] = obj;
                return make_handle(static_cast<std::uint32_t>(i + 1), next_instance++);
            }
        }

        objects.push_back(obj);
        return make_handle(static_cast<std::uint32_t>(objects.size()), next_instance++);
    }

    bool   object_table::remove(handle obj_handle) {
        const std::size_t idx = obj_handle & 0xFFFF;
        if (idx > objects.size() || idx == 0) {
            return false;
        }

        objects[idx - 1] = nullptr;
        return true;
    }

    ref_count_object *object_table::get_raw(handle obj_handle) {
        const std::size_t idx = obj_handle & 0xFFFF;
        if (idx > objects.size() || idx == 0) {
            return nullptr;
        }
        
        return objects[idx - 1];
    }

    void ref_count_object::ref() {
        count++;
    }

    void ref_count_object::deref() {
        count--;

        if (count == 0) {
            owner->remove(this);
        }
    }
}
