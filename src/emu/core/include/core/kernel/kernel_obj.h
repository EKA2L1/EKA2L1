#pragma once

#include <cstdint>
#include <string>

namespace eka2l1 {
    class kernel_system;

    namespace kernel {
        using uid = uint64_t;

        // Base class for all kernel object
        class kernel_obj {
        protected:
            std::string obj_name;
            uid obj_id;

            kernel_system *kern;

            kernel_obj(kernel_system *kern, const std::string &obj_name);
            kernel_obj(kernel_system *kern, const uid obj_id, const std::string &obj_name)
                : obj_id(obj_id)
                , obj_name(obj_name)
                , kern(kern) {}

        public:
            std::string name() const {
                return obj_name;
            }

            uid unique_id() const {
                return obj_id;
            }

            kernel_system *get_kernel_object_owner() const {
                return kern;
            }
        };
    }
}
