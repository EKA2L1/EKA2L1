#include <common/log.h>

#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

// Use for render text and draw custom bitmap
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_impl_opengl3.h"

namespace eka2l1::drivers {
    ogl_graphics_driver::ogl_graphics_driver(const vec2 &scr)
        : shared_graphics_driver(graphic_api::opengl, scr) {
        ImGui_ImplOpenGL3_Init(nullptr);
    }

    void ogl_graphics_driver::set_screen_size(const vec2 &s) {
        framebuffer->resize(s);
        framebuffer->bind();

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        framebuffer->unbind();
    }

    drivers::handle ogl_graphics_driver::upload_bitmap(drivers::handle h, const std::size_t size, 
        const std::uint32_t width, const std::uint32_t height, const int bpp, void *data) {
        // Bitmap data has each row aligned by 4, so set unpack alignment to 4 first
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        h = shared_graphics_driver::upload_bitmap(h, size, width, height, bpp, data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 0);

        return h;
    }
    
    bool ogl_graphics_driver::do_request_queue_execute_one_request(drivers::driver_request *request) {
        if (shared_graphics_driver::do_request_queue_execute_one_request(request)) {
            return true;
        }

        switch (request->opcode) {
        case graphics_driver_clear: {
            int c[4];

            for (std::size_t i = 0; i < 4; i++) {
                c[i] = *request->context.pop<int>();
            }

            glClearColor(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, c[3] / 255.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            break;
        }

        case graphics_driver_invalidate: {
            auto r = *request->context.pop<rect>();

            // Since it takes the lower left
            glScissor(r.top.x, framebuffer->get_size().y - (r.top.y + r.size.y),
                r.size.x, r.size.y);

            break;
        }

        default:
            return false; 
        }

        return true;
    }

    void ogl_graphics_driver::start_new_backend_frame() {
        ImGui_ImplOpenGL3_NewFrame();
    }

    void ogl_graphics_driver::render_frame(ImDrawData *draw_data) {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }
    
    void ogl_graphics_driver::do_request_queue_execute_job() {
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glScissor(0, 0, 0, 0);
        glEnable(GL_SCISSOR_TEST);

        glEnable(GL_DEPTH_TEST);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shared_graphics_driver::do_request_queue_execute_job();
    }

    void ogl_graphics_driver::do_request_queue_clean_job() {
        glDisable(GL_SCISSOR_TEST);
    }
}
