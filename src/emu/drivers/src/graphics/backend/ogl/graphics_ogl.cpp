#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

namespace eka2l1::drivers {
    ogl_graphics_driver::ogl_graphics_driver(const vec2 &scr) 
        : framebuffer(scr) {
    }

    ogl_graphics_driver::~ogl_graphics_driver() {
    }

    void ogl_graphics_driver::set_screen_size(const vec2 &s) {
        framebuffer.resize(s);
        framebuffer.bind();

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.3f, 0.4f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        framebuffer.unbind();
    }

    void ogl_graphics_driver::process_requests() {
        framebuffer.bind();

        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (;;) {
            auto request = request_queue.pop();

            if (!request) {
                break;
            }

            switch (request->opcode) {
            case graphics_driver_resize_screen: {
                framebuffer.unbind();
                
                auto v = *request->context.pop<vec2>();
                set_screen_size(v);

                framebuffer.bind();

                break;
            }

            case graphics_driver_clear: {
                auto c = *request->context.pop<vecx<float, 4>>();
                glClearColor(c[0], c[1], c[2], c[3]);

                break;
            }

            case graphics_driver_invalidate: {
                auto r = *request->context.pop<rect>();

                glScissor(r.top.x, r.top.y, r.size.x, r.size.y);
                glEnable(GL_SCISSOR_TEST);

                break;
            }

            case graphics_driver_end_invalidate: {
                glDisable(GL_SCISSOR_TEST);
                
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