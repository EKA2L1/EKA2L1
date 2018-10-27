#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

namespace eka2l1::drivers {
    ogl_graphics_driver::ogl_graphics_driver(const vec2 &scr) 
        : framebuffer(scr) {
    }

    ogl_graphics_driver::~ogl_graphics_driver() {
    }

    void ogl_graphics_driver::process_requests() {
        framebuffer.bind();

        for (;;) {
            auto request = request_queue.pop();

            if (!request) {
                break;
            }

            switch (request->opcode) {
            case graphics_driver_clear: {
                auto c = *request->context.pop<vecx<int, 4>>();
                glClearColor(c[0], c[1], c[2], c[3]);

                break;
            }

            default: {
                break;
            }
            }
        }

        cond.notify_all();
        framebuffer.unbind();
    }
}