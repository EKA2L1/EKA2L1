#include <core/core_kernel.h>
#include <core/kernel/object_ix.h>

#include <algorithm>

namespace eka2l1 {
    namespace kernel {
        handle_inspect_info inspect_handle(uint32_t handle) {
            handle_inspect_info info;

            if (handle >> 31 == 1) {
                // Special
                info.special = true;
                info.special_type = static_cast<special_handle_type>(handle);

                return info;
            } else {
                info.special = false;
            }

            if (((handle >> 30) & 0xf) == 1) {
                info.handle_array_local = true;
            } else {
                info.handle_array_local = false;
            }

            info.no_close = handle & 0x8000;
            info.object_ix_next_instance = (handle >> 16) & 0b0011111111111111;
            info.object_ix_index = handle & 0x7FFF;

            return info;
        }

        uint32_t object_ix::make_handle(size_t index) {
            uint32_t handle = 0;

            if (owner == handle_array_owner::thread) {
                // If handle array owner is thread, the 30th bit must be 1
                handle |= 0x40000000;
            }

            handle |= next_instance << 16;
            handle |= index;

            return handle;
        }

        uint32_t object_ix::add_object(kernel_obj_ptr obj) {
            auto &slot = std::find_if(objects.begin(), objects.end(),
                [](object_ix_record record) { return record.free; });

            if (slot != objects.end()) {
                next_instance++;
                uint32_t ret_handle = make_handle(slot - objects.begin());

                slot->associated_handle = ret_handle;
                slot->free = false;
                slot->object = obj;

                obj->increase_access_count();

                return ret_handle;
            }

            return INVALID_HANDLE;
        }

        uint32_t object_ix::duplicate(uint32_t handle) {
            handle_inspect_info info = inspect_handle(handle);

            if (!info.handle_array_local && owner == handle_array_owner::process) {
                return INVALID_HANDLE;
            }

            kernel_obj_ptr obj = get_object(handle);

            if (!obj) {
                return INVALID_HANDLE;
            }

            return add_object(obj);
        }

        kernel_obj_ptr object_ix::get_object(uint32_t handle) {
            handle_inspect_info info = inspect_handle(handle);

            if (info.object_ix_index < objects.size() && info.object_ix_next_instance < objects.size() - 1) {
                return objects[info.object_ix_index].object;
            }

            return nullptr;
        }

        bool object_ix::close(uint32_t handle) {
            handle_inspect_info info = inspect_handle(handle);

            if (info.object_ix_index < objects.size() && info.object_ix_next_instance < objects.size() - 1) {
                kernel_obj_ptr obj = objects[info.object_ix_index].object;
                obj->decrease_access_count();

                if (obj->get_access_count() <= 0) {
                    kern->destroy(obj);
                }

                objects[info.object_ix_index].free = true;

                return true;
            }

            return false;
        }

        object_ix::object_ix(kernel_system *kern, handle_array_owner owner)
            : kern(kern)
            , owner(owner)
            , next_instance(0)
            , uid(kern->next_uid()) {}
    }
}