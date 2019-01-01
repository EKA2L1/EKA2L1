#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

// Use for render text and draw custom bitmap
#include <imgui.h>
#include <imgui_internal.h>

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
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
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

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowBorderSize = 0.0f;

        /* Start new ImGui frame */
        ImGui_ImplOpenGL3_NewFrame();
        ImGuiIO &io = ImGui::GetIO();

        auto fb_size = framebuffer.get_size();

        io.DisplaySize = ImVec2(static_cast<float>(fb_size.x), static_cast<float>(fb_size.y));
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

        ImGui::NewFrame();

        /* When framebuffer change size, all elements must be redraw*/
        ImDrawList *draw_list = nullptr;

        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glScissor(0, 0, 0, 0);
        glEnable(GL_SCISSOR_TEST);

        glEnable(GL_DEPTH_TEST);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
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
                int c[4];

                for (std::size_t i = 0 ; i < 4; i ++) {
                    c[i] = *request->context.pop<int>();
                }

                glClearColor(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, c[3] / 255.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                break;
            }

            case graphics_driver_invalidate: {
                auto r = *request->context.pop<rect>();

                // Since it takes the lower left
                glScissor(r.top.x, framebuffer.get_size().y - (r.top.y + r.size.y),
                    r.size.x, r.size.y);

                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(io.DisplaySize);
                ImGui::Begin("BCKGND", NULL, io.DisplaySize, 0.0f, 
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus );

                draw_list = ImGui::GetWindowDrawList();

                break;
            }

            case graphics_driver_draw_text_box: {
                eka2l1::rect bound = *request->context.pop<rect>();
                std::string text = *request->context.pop_string();

                // TODO: Use the actual clip. Must calculate text size
                draw_list->AddText(nullptr, 6.0f, ImVec2(bound.top.x, bound.top.y), ImColor(0.0f, 0.0f, 0.0f, 1.0f), &text[0],
                    text.length() == 1 ? nullptr : &text[text.length() - 1]);

                break;
            }

            case graphics_driver_end_invalidate: {
                ImGui::End();
                break;
            }

            default: {
                break;
            }
            }
        }

        ImGui::EndFrame();
        ImGui::Render();

        ImDrawData *data = ImGui::GetDrawData();

        if (data) {
            ImGui_ImplOpenGL3_RenderDrawData(data);
        }

        ImGui::SetCurrentContext(last_context);

        glDisable(GL_SCISSOR_TEST);

        cond.notify_all();
        framebuffer.unbind();
    }
}