#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <glad/glad.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

// Use for render text and draw custom bitmap
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_impl_opengl3.h"

namespace eka2l1::drivers {
    ogl_window::ogl_window(const eka2l1::vec2 &size, const std::uint32_t pri, bool visible)
        : fb(size), pri(pri), visible(visible) {

    }
    
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

    void ogl_graphics_driver::redraw_window_list() {
        assert(!binding && "A window is currently being binded, not able to redraw window list!");

        ImGui::NewFrame();              
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("BCKGND", NULL, ImGui::GetIO().DisplaySize, 0.0f, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus );

        ImVec2 base_pos = ImGui::GetCursorScreenPos();

        for (auto &window: windows) {
            if (window->visible) {
                const eka2l1::vec2 win_size = window->fb.get_size();
                ImVec2 win_pos = ImVec2(static_cast<float>(window->pos.x), static_cast<float>(window->pos.y));

                ImGui::GetWindowDrawList()->AddImage((ImTextureID)window->fb.texture_handle(),
                    ImVec2(base_pos.x + win_pos.x,
                    base_pos.y + win_pos.y),

                    ImVec2(base_pos.x + win_pos.x + win_size.x, 
                    base_pos.y + win_pos.y + win_size.y), 
                    ImVec2(0, 1), ImVec2(1, 0));
            }
        }

        ImGui::End();
        ImGui::EndFrame();
        
        ImDrawData *data = ImGui::GetDrawData();

        if (data) {
            ImGui_ImplOpenGL3_RenderDrawData(data);
        }
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
            case graphics_driver_create_window: {
                eka2l1::vec2 size = *request->context->pop<vec2>();
                std::uint32_t pri = *request->context->pop<std::uint32_t>();
                bool visible = *request->context->pop<std::uint32_t>();

                drivers::ogl_window_ptr win = std::make_unique<ogl_window>(size, pri, visible);
                win->id = id_counter++;

                request->context->push(win->id);
                windows.push(std::move(win));

                break;
            }

            case graphics_driver_begin_window: {
                std::uint32_t id = *request->context->pop<std::uint32_t>();
                auto win_ite = std::find_if(windows.begin(), windows.end(),
                    [=](const ogl_window_ptr &win) { return win->id == id; });

                if (win_ite == windows.end()) {
                    break;
                }

                // Unbind the current framebuffer
                framebuffer.unbind();
                (*win_ite)->fb.bind();

                ImGui::NewFrame();

                eka2l1::vec2 win_fb_size = (*win_ite)->fb.get_size();
                ImVec2 overlay_size(static_cast<float>(win_fb_size.x), static_cast<float>(win_fb_size.y));

                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(overlay_size);
                ImGui::Begin("BCKGND", NULL, overlay_size, 0.0f, 
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus );

                draw_list = ImGui::GetWindowDrawList();

                binding = *win_ite;
            }

            case graphics_driver_end_window: {
                assert(binding && "No window are currently binded");

                ImGui::End();
                ImGui::EndFrame();
                ImGui::Render();

                ImDrawData *data = ImGui::GetDrawData();

                if (data) {
                    ImGui_ImplOpenGL3_RenderDrawData(data);
                }

                binding->fb.unbind();
                framebuffer.bind();

                binding.reset();
                draw_list = nullptr;
            }

            case graphics_driver_resize_screen: {
                assert(binding && "Sanity check, discovered that currently a window is binded. Unbind to resize.");  
                framebuffer.unbind();
                
                auto v = *request->context->pop<vec2>();
                set_screen_size(v);

                framebuffer.bind();

                break;
            }

            case graphics_driver_destroy_window: {
                std::uint32_t id = *request->context->pop<std::uint32_t>();
                auto win_ite = std::find_if(windows.begin(), windows.end(),
                    [=](const ogl_window_ptr &win) { return win->id == id; });

                if (win_ite == windows.end()) {
                    break;
                }

                windows.remove(*win_ite);

                break;
            }

            case graphics_driver_set_window_size: {
                std::uint32_t id = *request->context->pop<std::uint32_t>();
                auto win_ite = std::find_if(windows.begin(), windows.end(),
                    [=](const ogl_window_ptr &win) { return win->id == id; });

                if (win_ite == windows.end()) {
                    break;
                }

                (*win_ite)->fb.resize(*request->context->pop<vec2>());
                break;
            }
            
            case graphics_driver_set_priority: {
                std::uint32_t id = *request->context->pop<std::uint32_t>();
                auto win_ite = std::find_if(windows.begin(), windows.end(),
                    [=](const ogl_window_ptr &win) { return win->id == id; });

                if (win_ite == windows.end()) {
                    break;
                }

                (*win_ite)->pri = (*request->context->pop<std::uint32_t>());
                windows.resort();

                break;
            }

            case graphics_driver_set_visibility: {
                std::uint32_t id = *request->context->pop<std::uint32_t>();
                auto win_ite = std::find_if(windows.begin(), windows.end(),
                    [=](const ogl_window_ptr &win) { return win->id == id; });

                if (win_ite == windows.end()) {
                    break;
                }

                (*win_ite)->visible = (*request->context->pop<bool>());
                break;
            }

            case graphics_driver_clear: {
                int c[4];

                for (std::size_t i = 0 ; i < 4; i ++) {
                    c[i] = *request->context->pop<int>();
                }

                glClearColor(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, c[3] / 255.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                break;
            }

            case graphics_driver_invalidate: {
                auto r = *request->context->pop<rect>();

                // Since it takes the lower left
                glScissor(r.top.x, framebuffer.get_size().y - (r.top.y + r.size.y),
                    r.size.x, r.size.y);
                
                break;
            }

            case graphics_driver_draw_text_box: {
                eka2l1::rect bound = *request->context->pop<rect>();
                std::string text = *request->context->pop_string();

                // TODO: Use the actual clip. Must calculate text size
                draw_list->AddText(nullptr, 6.0f, ImVec2(bound.top.x, bound.top.y), ImColor(0.0f, 0.0f, 0.0f, 1.0f), &text[0],
                    text.length() == 1 ? nullptr : &text[text.length() - 1]);

                break;
            }

            case graphics_driver_end_invalidate: {
                should_rerender = true;
                break;
            }

            default: {
                break;
            }
            }
        }

        if (should_rerender) {
            redraw_window_list();
            should_rerender = false;
        }

        ImGui::SetCurrentContext(last_context);
        glDisable(GL_SCISSOR_TEST);

        cond.notify_all();
        framebuffer.unbind();
    }
}