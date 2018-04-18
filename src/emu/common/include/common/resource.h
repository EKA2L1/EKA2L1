#pragma once

#include <functional>

namespace eka2l1 {
    namespace common {

        // Custom runtime RAII
    \   template <typename T>
        struct resource {
            using deleter = std::function<void(T)>;

            T res;
            deleter del;

        public:
            resource(T res, deleter del)
                : res(res), del(del) {}

            ~resource() {
                if (del) {
                    del(res);
                }
            }

            T get() const {
                return res;
            }
        };
    }
}
