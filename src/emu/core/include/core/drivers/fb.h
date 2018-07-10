#pragma once

#include <memory>

namespace eka2l1::driver {
    class instance;
    using instance_ptr = std::unique_ptr<instance>;

    class framebuffer {
    public:
        virtual void init(instance_ptr &instance) = 0;
        virtual void deinit() = 0;

        virtual void bind() = 0;
        virtual void unbind() = 0;

        virtual void draw() = 0;

        virtual void set_pixel(uint16_t pixel) = 0;
        virtual void get_pixel(uint16_t pixel) = 0;
    };
}