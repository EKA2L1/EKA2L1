#pragma once

#include <cstdint>
#include <string>

namespace eka2l1 {
    namespace kernel {
        using uid = uint64_t;

        // Base class for all kernel object
        class kernel_obj {
        protected:
            std::string obj_name;
            uid obj_id;

            kernel_obj(const uid obj_id, const std::string& obj_name)
                : obj_id(obj_id), obj_name(obj_name) {}

        public:
            std::string name() const {
                return obj_name;
            }

            uid unique_id() const {
                return obj_id;
            }
        };
    }
}
