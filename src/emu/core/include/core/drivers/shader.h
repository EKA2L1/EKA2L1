#pragma once

#include <cstdint>
#include <string>

namespace eka2l1 {
    namespace driver {
        enum class shader_type {
            vert,
            frag
        };

        class shader {
        public:
            virtual bool load(const char *raw_data, size_t data_size, const shader_type type) = 0;
            virtual bool compile() = 0;

            bool load(const std::string &path, const shader_type type);
        };
    }
}