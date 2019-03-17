/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/log.h>
#include <drivers/graphics/backend/graphics_driver_shared.h>

// Use for render text and draw custom bitmap
#include <imgui.h>
#include <imgui_internal.h>

#include <glad/glad.h>

namespace eka2l1::drivers {
    embed_window::embed_window(const graphic_api gr_api, const eka2l1::vec2 &size, const std::uint16_t pri, bool visible)
        : pri(pri)
        , visible(visible) {
        fb = make_framebuffer(gr_api, size);
    }

    shared_graphics_driver::shared_graphics_driver(const graphic_api gr_api, const vec2 &scr)
        : gr_api_(gr_api) {
        do_init(gr_api, scr);
    }

    shared_graphics_driver::~shared_graphics_driver() {
        ImGui::DestroyContext(context);
        irenderer->deinit();
    }

    void shared_graphics_driver::do_init(const graphic_api gr_api, const vec2 &scr) {
        gr_api_ = gr_api;
        
        framebuffer = make_framebuffer(gr_api, scr);
        context = ImGui::CreateContext();

        auto last_context = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context);

        irenderer = make_imgui_renderer(gr_api);
        irenderer->init();

        ImGui::SetCurrentContext(last_context);
    }

    void shared_graphics_driver::render_frame(ImDrawData *draw_data) {
        irenderer->render(draw_data);
    }

    drivers::handle shared_graphics_driver::upload_bitmap(drivers::handle h, const std::size_t size, 
        const std::uint32_t width, const std::uint32_t height, const int bpp, void *data) {
        if (bpp != 24 && bpp != 32) {
            LOG_TRACE("Unsupported bit per pixels: {}", bpp);
            return 0;
        }

        drivers::texture_format tex_format;
        drivers::texture_format internal_tex_format;
        drivers::texture_data_type tex_data_type;

        // Get the upload format
        switch (bpp) {
        case 24: {
            tex_format = drivers::texture_format::bgr;
            internal_tex_format = drivers::texture_format::rgba;
            tex_data_type = drivers::texture_data_type::ubyte;
            break;
        }

        case 32: {
            tex_format = drivers::texture_format::rgba; 
            internal_tex_format = tex_format;
            tex_data_type = drivers::texture_data_type::ubyte;
            break;
        }

        default: 
            break;
        }

        texture_ptr btex = nullptr;
        
        // Create a new texture if it doesn't exist
        if (h != 0) {
            // Iterate through all available textures
            auto result = std::lower_bound(bmp_textures.begin(), bmp_textures.end(), h,
                [=](const texture_ptr &tex, const drivers::handle &h) {
                    return tex->texture_handle() < h;
                });

            // Can't find it.
            if (result == bmp_textures.end()) {
                return 0;
            }

            btex = *result;
            btex->change_data(tex_data_type, data);
            btex->change_size(vec3(width, height, 0));
            btex->change_texture_format(tex_format);

            btex->bind();
            btex->tex(false);
            btex->unbind();
        }
    
        if (btex == nullptr) {
            btex = make_texture(gr_api_);
            btex->create(2, 0, vec3(width, height, 0), internal_tex_format, tex_format,
                tex_data_type, data);

            btex->set_filter_minmag(true, drivers::filter_option::linear);
            btex->set_filter_minmag(false, drivers::filter_option::linear);

            h = btex->texture_handle();
            bmp_textures.push_back(std::move(btex));

            std::stable_sort(bmp_textures.begin(), bmp_textures.end(), [](const texture_ptr &lhs, const texture_ptr &rhs) {
                return lhs->texture_handle() < rhs->texture_handle();
            });
        }

        return h;
    }
    
    bool shared_graphics_driver::do_request_queue_execute_one_request(drivers::driver_request *request) {
        switch (request->opcode) {
        case graphics_driver_create_window: {
            eka2l1::vec2 size = *request->context.pop<vec2>();
            std::uint16_t pri = *request->context.pop<std::uint16_t>();
            bool visible = *request->context.pop<bool>();

            framebuffer->unbind();

            drivers::embed_window_ptr win = std::make_unique<embed_window>(gr_api_, size, pri, visible);
            win->id = id_counter++;

            std::uint32_t *data_ret = *request->context.pop<std::uint32_t *>();
            *data_ret = win->id;

            windows.push(std::move(win));

            framebuffer->bind();

            break;
        }

        case graphics_driver_begin_window: {
            std::uint32_t id = *request->context.pop<std::uint32_t>();
            auto win_ite = std::find_if(windows.begin(), windows.end(),
                [=](const embed_window_ptr &win) { return win->id == id; });

            if (win_ite == windows.end()) {
                break;
            }

            // Unbind the current framebuffer
            (*win_ite)->fb->bind();

            ImGui::NewFrame();

            eka2l1::vec2 win_fb_size = (*win_ite)->fb->get_size();
            ImVec2 overlay_size(static_cast<float>(win_fb_size.x), static_cast<float>(win_fb_size.y));

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(overlay_size);
            ImGui::Begin("BCKGND", NULL, overlay_size, 0.0f,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

            draw_list = ImGui::GetWindowDrawList();

            binding = *win_ite;
            break;
        }

        case graphics_driver_end_window: {
            assert(binding && "No window are currently binded");

            ImGui::End();
            ImGui::EndFrame();
            ImGui::Render();

            ImDrawData *data = ImGui::GetDrawData();

            if (data) {
                render_frame(data);
            }

            binding->fb->unbind();

            binding.reset();
            draw_list = nullptr;

            break;
        }

        case graphics_driver_resize_screen: {
            assert(!binding && "Sanity check, discovered that currently a window is binded. Unbind to resize.");
            framebuffer->unbind();

            auto v = *request->context.pop<vec2>();
            set_screen_size(v);

            framebuffer->bind();

            break;
        }

        case graphics_driver_destroy_window: {
            std::uint32_t id = *request->context.pop<std::uint32_t>();
            auto win_ite = std::find_if(windows.begin(), windows.end(),
                [=](const embed_window_ptr &win) { return win->id == id; });

            if (win_ite == windows.end()) {
                break;
            }

            windows.remove(*win_ite);

            break;
        }

        case graphics_driver_set_window_size: {
            std::uint32_t id = *request->context.pop<std::uint32_t>();
            auto win_ite = std::find_if(windows.begin(), windows.end(),
                [=](const embed_window_ptr &win) { return win->id == id; });

            if (win_ite == windows.end()) {
                break;
            }

            (*win_ite)->fb->resize(*request->context.pop<vec2>());
            break;
        }

        case graphics_driver_set_priority: {
            std::uint32_t id = *request->context.pop<std::uint32_t>();
            auto win_ite = std::find_if(windows.begin(), windows.end(),
                [=](const embed_window_ptr &win) { return win->id == id; });

            if (win_ite == windows.end()) {
                break;
            }

            (*win_ite)->pri = (*request->context.pop<std::uint16_t>());
            windows.resort();

            break;
        }

        case graphics_driver_set_visibility: {
            std::uint32_t id = *request->context.pop<std::uint32_t>();
            auto win_ite = std::find_if(windows.begin(), windows.end(),
                [=](const embed_window_ptr &win) { return win->id == id; });

            if (win_ite == windows.end()) {
                break;
            }

            (*win_ite)->visible = (*request->context.pop<bool>());
            break;
        }

        case graphics_driver_set_win_pos: {
            std::uint32_t id = *request->context.pop<std::uint32_t>();
            auto win_ite = std::find_if(windows.begin(), windows.end(),
                [=](const embed_window_ptr &win) { return win->id == id; });

            if (win_ite == windows.end()) {
                break;
            }

            (*win_ite)->pos = (*request->context.pop<vec2>());
            break;
        }

        case graphics_driver_draw_text_box: {
            eka2l1::rect bound = *request->context.pop<rect>();
            std::string text = *request->context.pop_string();

            // TODO: Use the actual clip. Must calculate text size
            draw_list->AddText(nullptr, 6.0f, ImVec2(static_cast<float>(bound.top.x), static_cast<float>(bound.top.y)),
                ImColor(0.0f, 0.0f, 0.0f, 1.0f), &text[0], text.length() == 1 ? nullptr : &text[text.length() - 1]);

            break;
        }

        case graphics_driver_draw_bitmap: {
            drivers::handle bmp_handle = *request->context.pop<drivers::handle>();
            eka2l1::rect rect = *request->context.pop<eka2l1::rect>();

            draw_list->AddImage(
                reinterpret_cast<ImTextureID>(bmp_handle),
                ImVec2(static_cast<float>(rect.top.x), static_cast<float>(rect.top.y)),
                ImVec2(static_cast<float>(rect.top.x + rect.size.width()), 
                       static_cast<float>(rect.top.y + rect.size.height())),
                ImVec2(0, 1), ImVec2(1, 0));

            break;
        }

        case graphics_driver_upload_bitmap: {
            const std::size_t size = *request->context.pop<std::size_t>(); 
            const std::uint32_t width = *request->context.pop<std::uint32_t>();
            const std::uint32_t height = *request->context.pop<std::uint32_t>();
            const int bpp = *request->context.pop<int>();
            void *data = *request->context.pop<void*>();

            drivers::handle *handle_ptr = *request->context.pop<drivers::handle*>();
            *handle_ptr = upload_bitmap(*handle_ptr, size, width, height, bpp, data);

            break;
        }

        default: {
            return false;
        }
        }

        request->finish(0);
        return true;
    }

    void shared_graphics_driver::do_request_queue_execute_job() {
        /* When framebuffer change size, all elements must be redraw*/
        draw_list = nullptr;

        for (;;) {
            auto request = request_queue.pop();

            if (!request) {
                break;
            }

            do_request_queue_execute_one_request(&(request.value()));
        }
    }

    void shared_graphics_driver::redraw_window_list() {
        assert(!binding && "A window is currently being binded, not able to redraw window list!");

        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("BCKGND", NULL, ImGui::GetIO().DisplaySize, 0.0f,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | 
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | 
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImVec2 base_pos = ImVec2(0.0f, 0.0f);

        for (auto &window : windows) {
            if (window->visible) {
                const eka2l1::vec2 win_size = window->fb->get_size();
                ImVec2 win_pos = ImVec2(static_cast<float>(window->pos.x), static_cast<float>(window->pos.y));

                ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(
                    static_cast<std::uint64_t>(window->fb->texture_handle())),
                    ImVec2(base_pos.x + win_pos.x,
                        base_pos.y + win_pos.y),
                    ImVec2(base_pos.x + win_pos.x + win_size.x,
                        base_pos.y + win_pos.y + win_size.y),
                    ImVec2(0, 1), ImVec2(1, 0));
            }
        }

        ImGui::End();
        ImGui::EndFrame();
        
        ImGui::Render();

        ImDrawData *data = ImGui::GetDrawData();

        if (data) {
            render_frame(data);
        }
    }

    void shared_graphics_driver::process_requests() {        
        framebuffer->bind();

        auto last_context = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context);

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowBorderSize = 0.0f;

        /* Start new ImGui frame */
        start_new_backend_frame();
        ImGuiIO &io = ImGui::GetIO();

        auto fb_size = framebuffer->get_size();

        io.DisplaySize = ImVec2(static_cast<float>(fb_size.x), static_cast<float>(fb_size.y));
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

        do_request_queue_execute_job();

        if (should_rerender) {
            redraw_window_list();
            should_rerender = false;
        }
        
        ImGui::SetCurrentContext(last_context);
        do_request_queue_clean_job();

        cond.notify_all();
        framebuffer->unbind();
    }
}
