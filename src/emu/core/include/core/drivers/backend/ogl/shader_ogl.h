#pragma once

#include <core/drivers/shader.h>

#include <cstdint>
#include <string>

namespace eka2l1 {
    namespace driver {
        class shader_ogl: public shader {
            int shader_handle;
        public:
            bool load(const char *raw_data, size_t data_size, const shader_type type) override;
            bool compile() override;
        };
    }
}