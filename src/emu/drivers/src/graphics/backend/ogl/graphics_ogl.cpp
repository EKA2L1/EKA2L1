#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

// Use for render text and draw custom bitmap
#include <imgui.h>
#include "imgui_impl_opengl3.h"

namespace eka2l1::drivers {
    ogl_graphics_driver::ogl_graphics_driver(const vec2 &scr) 
        : framebuffer(scr) {
        ImGui_ImplOpenGL3_Init(nullptr);
        context = ImGui::CreateContext();
    }

    ogl_graphics_driver::~ogl_graphics_driver() {
        ImGui::DestroyContext(context);
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

        auto last_context = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context);

        /* Start new ImGui frame */
        ImGui_ImplOpenGL3_NewFrame();
        ImGuiIO &io = ImGui::GetIO();

        auto fb_size = framebuffer.get_size();

        io.DisplaySize = ImVec2(static_cast<float>(fb_size.x), static_cast<float>(fb_size.y));
        io.DisplayFramebufferScale = io.DisplayFramebufferScale;

        ImGui::NewFrame();

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

        ImGui::EndFrame();

        ImDrawData *data = ImGui::GetDrawData();

        if (data) {
            ImGui_ImplOpenGL3_RenderDrawData(data);
        }

        ImGui::SetCurrentContext(last_context);

        cond.notify_all();
        framebuffer.unbind();
    }
}