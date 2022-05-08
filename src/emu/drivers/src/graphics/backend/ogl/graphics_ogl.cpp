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

#include <common/algorithm.h>
#include <common/log.h>
#include <common/platform.h>
#include <common/rgb.h>
#include <fstream>
#include <sstream>

#include <drivers/graphics/backend/ogl/common_ogl.h>
#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <drivers/graphics/backend/ogl/buffer_ogl.h>
#include <glad/glad.h>

#if EKA2L1_PLATFORM(ANDROID)
#include <EGL/egl.h>
#endif

#define IMGUI_IMPL_OPENGL_LOADER_GLAD

namespace eka2l1::drivers {
    static void gl_post_callback_for_error(const char *name, void *funcptr, int len_args, ...) {
        GLenum error_code;
        error_code = glad_glGetError();

        if (error_code != GL_NO_ERROR) {
            LOG_ERROR(DRIVER_GRAPHICS, "{} encounters error {}", name, error_code);
        }
    }

    void init_gl_graphics_library(graphics::gl_context::mode api) {
        switch (api) {
            case graphics::gl_context::mode::opengl: {
                gladLoadGL();
                break;
            }

            case graphics::gl_context::mode::opengl_es: {
#if EKA2L1_PLATFORM(ANDROID)
                if (!gladLoadGLES2Loader((GLADloadproc) eglGetProcAddress)) {
                    LOG_CRITICAL(DRIVER_GRAPHICS, "gladLoadGLES2Loader() failed");
                    return;
                }
#endif

                break;
            }

            default:
                return;
        }

        glad_set_post_callback(gl_post_callback_for_error);
    }

    ogl_graphics_driver::ogl_graphics_driver(const window_system_info &info)
        : shared_graphics_driver(graphic_api::opengl)
        , should_stop(false)
        , surface_update_needed(false)
        , new_surface(nullptr)
        , is_gles(false)
        , support_line_width_(true)
        , point_size(1.0)
        , line_style(pen_style_none)
        , active_input_descriptors_(nullptr)
        , index_buffer_current_(0)
        , feature_flags_(0)
        , active_upscale_shader_("Default") {
        context_ = graphics::make_gl_context(info, false, true);

        if (!context_) {
            LOG_ERROR(DRIVER_GRAPHICS, "OGL context failed to create!");
            return;
        }

        init_gl_graphics_library(context_->gl_mode());
        list_queue.max_pending_count_ = 128;

        context_->set_swap_interval(1);

        is_gles = (context_->gl_mode() == graphics::gl_context::mode::opengl_es);

        GLint major_gl = 0;
        GLint minor_gl = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major_gl);
        glGetIntegerv(GL_MINOR_VERSION, &minor_gl);

        if (!is_gles && (major_gl < 4)) {
            std::vector<std::string> GL_REQUIRED_EXTENSIONS = {
                "GL_ARB_draw_elements_base_vertex"
            };

            std::int32_t ext_count = 0;
            glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

            for (std::int32_t i = 0; i < ext_count; i++) {
                const GLubyte *next_extension = glGetStringi(GL_EXTENSIONS, i);
                auto ite = std::find(GL_REQUIRED_EXTENSIONS.begin(), GL_REQUIRED_EXTENSIONS.end(),
                    std::string(reinterpret_cast<const char *>(next_extension)));

                if (ite != GL_REQUIRED_EXTENSIONS.end()) {
                    GL_REQUIRED_EXTENSIONS.erase(ite);
                }
            }

            if (!GL_REQUIRED_EXTENSIONS.empty()) {
                LOG_ERROR(DRIVER_GRAPHICS, "Some OpenGL extensions are missing in order for the emulator to work.");
                LOG_ERROR(DRIVER_GRAPHICS, "Please upgrade your machine! Here is the list of missing extensions.");

                for (const auto &ext_left : GL_REQUIRED_EXTENSIONS) {
                    LOG_ERROR(DRIVER_GRAPHICS, "- {}", ext_left);
                }
            }
        } else {
            if ((minor_gl >= 3) || is_gles) {
                feature_flags_ |= OGL_FEATURE_SUPPORT_ETC2;
            }
        }

        std::int32_t ext_count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

        for (std::int32_t i = 0; i < ext_count; i++) {
            const GLubyte *next_extension = glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(reinterpret_cast<const char*>(next_extension), "GL_ARB_ES3_compatibility") == 0) {
                feature_flags_ |= OGL_FEATURE_SUPPORT_ETC2;
            }

            if (strcmp(reinterpret_cast<const char*>(next_extension), "GL_IMG_texture_compression_pvrtc") == 0) {
                feature_flags_ |= OGL_FEATURE_SUPPORT_PVRTC;
            }

            if (strcmp(reinterpret_cast<const char*>(next_extension), "GL_EXT_texture_filter_anisotropic") == 0) {
                feature_flags_ |= OGL_FEATURE_SUPPORT_ANISOTROPHY;

                anisotrophy_max_ = 1.0f;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotrophy_max_);
            }
        }

        std::string feature = "";

        if (feature_flags_ & OGL_FEATURE_SUPPORT_ETC2) {
            feature += "ETC2Dec;";
        }

        if (feature_flags_ & OGL_FEATURE_SUPPORT_PVRTC) {
            feature += "PVRTDec;";
        }

        if (feature_flags_ & OGL_FEATURE_SUPPORT_ANISOTROPHY) {
            feature += "AnisotrophyFiltering;";
        }

        if (!feature.empty()) {
            feature.pop_back();
        }

        LOG_TRACE(DRIVER_GRAPHICS, "Supported features (for optimization): {}", feature);
    }

    ogl_graphics_driver::~ogl_graphics_driver() {
        bmp_textures.clear();
        graphic_objects.clear();

        sprite_program.reset();
        brush_program.reset();
        mask_program.reset();
        pen_program.reset();

        GLuint vao_to_del[3] = { sprite_vao, brush_vao, pen_vao };
        GLuint vbo_to_del[3] = { sprite_vbo, brush_vbo, pen_vbo };
        GLuint ibo_to_del[2] = { sprite_ibo, pen_ibo };

        glDeleteVertexArrays(3, vao_to_del);
        glDeleteBuffers(3, vbo_to_del);
        glDeleteBuffers(2, ibo_to_del);
    }

    bool ogl_graphics_driver::support_extension(const graphics_driver_extension ext) {
        if (ext == graphics_driver_extension_anisotrophy_filtering) {
            return (feature_flags_ & OGL_FEATURE_SUPPORT_ANISOTROPHY);
        }

        return false;
    }

    bool ogl_graphics_driver::query_extension_value(const graphics_driver_extension_query query, void *data_ptr) {
        if (!data_ptr) {
            return false;
        }

        switch (query) {
        case graphics_driver_extension_query_max_texture_max_anisotrophy:
            if ((feature_flags_ & OGL_FEATURE_SUPPORT_ANISOTROPHY) == 0) {
                return false;
            }
            
            *reinterpret_cast<float*>(data_ptr) = anisotrophy_max_;

            break;

        default:
            return false;
        }

        return true;
    }

    static constexpr const char *sprite_norm_v_path = "resources//sprite_norm.vert";
    static constexpr const char *sprite_norm_f_path = "resources//sprite_norm.frag";
    static constexpr const char *sprite_mask_f_path = "resources//sprite_mask.frag";
    static constexpr const char *sprite_upscaled_f_path = "resources//sprite_upscaled.frag";
    static constexpr const char *brush_v_path = "resources//brush.vert";
    static constexpr const char *brush_f_path = "resources//brush.frag";
    static constexpr const char *pen_v_path = "resources//pen.vert";
    static constexpr const char *pen_f_path = "resources//pen.frag";

    void ogl_graphics_driver::do_init() {
        auto sprite_norm_vertex_module = std::make_unique<ogl_shader_module>(sprite_norm_v_path, shader_module_type::vertex);        
        auto brush_vertex_module = std::make_unique<ogl_shader_module>(brush_v_path, shader_module_type::vertex);
        auto pen_vertex_module = std::make_unique<ogl_shader_module>(pen_v_path, shader_module_type::vertex);

        auto sprite_norm_fragment_module = std::make_unique<ogl_shader_module>(sprite_norm_f_path, shader_module_type::fragment);
        auto sprite_mask_fragment_module = std::make_unique<ogl_shader_module>(sprite_mask_f_path, shader_module_type::fragment);
        auto sprite_upscale_fragment_module = std::make_unique<ogl_shader_module>(sprite_upscaled_f_path, shader_module_type::fragment);
        auto brush_fragment_module = std::make_unique<ogl_shader_module>(brush_f_path, shader_module_type::fragment);
        auto pen_fragment_module = std::make_unique<ogl_shader_module>(pen_f_path, shader_module_type::fragment);

        sprite_program = std::make_unique<ogl_shader_program>();
        mask_program = std::make_unique<ogl_shader_program>();
        brush_program = std::make_unique<ogl_shader_program>();
        pen_program = std::make_unique<ogl_shader_program>();
        upscale_program = std::make_unique<ogl_shader_program>();

        sprite_program->create(this, sprite_norm_vertex_module.get(), sprite_norm_fragment_module.get());
        mask_program->create(this, sprite_norm_vertex_module.get(), sprite_mask_fragment_module.get());
        upscale_program->create(this, sprite_norm_vertex_module.get(), sprite_upscale_fragment_module.get());
        brush_program->create(this, brush_vertex_module.get(), brush_fragment_module.get());
        pen_program->create(this, pen_vertex_module.get(), pen_fragment_module.get());

        for (int i = 0; i < GL_BACKEND_MAX_VBO_SLOTS; i++) {
            vbo_slots_[i] = 0;
        }

        static GLushort indices[] = {
            0, 1, 2,
            0, 3, 1
        };

        // Make sprite VAO and VBO
        glGenVertexArrays(1, &sprite_vao);
        glGenBuffers(1, &sprite_vbo);
        glBindVertexArray(sprite_vao);
        glBindBuffer(GL_ARRAY_BUFFER, sprite_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)(2 * sizeof(GLfloat)));
        glBindVertexArray(0);

        // Make fill VAO and VBO
        static GLfloat brush_verts_default[] = {
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f
        };

        glGenVertexArrays(1, &brush_vao);
        glGenBuffers(1, &brush_vbo);
        glBindVertexArray(brush_vao);
        glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(brush_verts_default), nullptr, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(brush_verts_default), brush_verts_default, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);
        glBindVertexArray(0);

        // Make pen VAO and VBO
        glGenVertexArrays(1, &pen_vao);
        glGenBuffers(1, &pen_vbo);
        glBindVertexArray(pen_vao);
        glBindBuffer(GL_ARRAY_BUFFER, pen_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);
        glBindVertexArray(0);

        glGenBuffers(1, &sprite_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sprite_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glGenBuffers(1, &pen_ibo);

        color_loc = sprite_program->get_uniform_location("u_color").value_or(-1);
        proj_loc = sprite_program->get_uniform_location("u_proj").value_or(-1);
        model_loc = sprite_program->get_uniform_location("u_model").value_or(-1);
        in_position_loc = sprite_program->get_attrib_location("in_position").value_or(-1);
        in_texcoord_loc = sprite_program->get_attrib_location("in_texcoord").value_or(-1);

        color_upscaled_loc = upscale_program->get_uniform_location("u_color").value_or(-1);
        proj_upscaled_loc = upscale_program->get_uniform_location("u_proj").value_or(-1);
        model_upscaled_loc = upscale_program->get_uniform_location("u_model").value_or(-1);
        texel_delta_upscaled_loc_ = upscale_program->get_uniform_location("u_texelDelta").value_or(-1);
        pixel_delta_upscaled_loc_ = upscale_program->get_uniform_location("u_pixelDelta").value_or(-1);
        in_position_loc_upscale = upscale_program->get_attrib_location("in_position").value_or(-1);
        in_texcoord_loc_upscale = upscale_program->get_attrib_location("in_texcoord").value_or(-1);

        color_loc_brush = brush_program->get_uniform_location("u_color").value_or(-1);
        proj_loc_brush = brush_program->get_uniform_location("u_proj").value_or(-1);
        model_loc_brush = brush_program->get_uniform_location("u_model").value_or(-1);

        color_loc_mask = mask_program->get_uniform_location("u_color").value_or(-1);
        proj_loc_mask = mask_program->get_uniform_location("u_proj").value_or(-1);
        model_loc_mask = mask_program->get_uniform_location("u_model").value_or(-1);
        invert_loc_mask = mask_program->get_uniform_location("u_invert").value_or(-1);
        source_loc_mask = mask_program->get_uniform_location("u_tex").value_or(-1);
        mask_loc_mask = mask_program->get_uniform_location("u_mask").value_or(-1);
        flat_blend_loc_mask = mask_program->get_uniform_location("u_flat").value_or(-1);
        in_position_loc_mask = mask_program->get_attrib_location("in_position").value_or(-1);
        in_texcoord_loc_mask = mask_program->get_attrib_location("in_texcoord").value_or(-1);
        
        color_loc_pen = pen_program->get_uniform_location("u_color").value_or(-1);
        proj_loc_pen = pen_program->get_uniform_location("u_proj").value_or(-1);
        model_loc_pen = pen_program->get_uniform_location("u_model").value_or(-1);
        point_size_loc_pen = pen_program->get_uniform_location("u_pointSize").value_or(-1);
        pattern_bytes_loc_pen = pen_program->get_uniform_location("u_pattern").value_or(-1);
        viewport_loc_pen = pen_program->get_uniform_location("u_viewport").value_or(-1);
    }

    void ogl_graphics_driver::commit_upscale_shader_change() {
        if (pending_upscale_shader_.empty()) {
            return;
        }

        std::string extra_header = "";

        if ((pending_upscale_shader_ == "default") || (pending_upscale_shader_ == "Default")) {
            pending_upscale_shader_ = sprite_upscaled_f_path;
        } else {
            pending_upscale_shader_ = "resources//upscale//" + pending_upscale_shader_ + ".frag";
            if (is_gles) {
                extra_header = "#version 300 es\n";
            } else {
                extra_header = "#version 140\n";
            }
        }

        auto sprite_norm_vertex_module = std::make_unique<ogl_shader_module>(sprite_norm_v_path, shader_module_type::vertex);        
        auto sprite_upscale_fragment_module = std::make_unique<ogl_shader_module>(pending_upscale_shader_, shader_module_type::fragment, extra_header);

        auto upscale_program_new = std::make_unique<ogl_shader_program>();

        if (!upscale_program_new->create(this, sprite_norm_vertex_module.get(), sprite_upscale_fragment_module.get())) {
            pending_upscale_shader_.clear();
            return;
        }

        upscale_program = std::move(upscale_program_new);

        color_upscaled_loc = upscale_program->get_uniform_location("u_color").value_or(-1);
        proj_upscaled_loc = upscale_program->get_uniform_location("u_proj").value_or(-1);
        model_upscaled_loc = upscale_program->get_uniform_location("u_model").value_or(-1);
        texel_delta_upscaled_loc_ = upscale_program->get_uniform_location("u_texelDelta").value_or(-1);
        pixel_delta_upscaled_loc_ = upscale_program->get_uniform_location("u_pixelDelta").value_or(-1);
        in_position_loc_upscale = upscale_program->get_attrib_location("in_position").value_or(-1);
        in_texcoord_loc_upscale = upscale_program->get_attrib_location("in_texcoord").value_or(-1);

        pending_upscale_shader_.clear();
    }

    void ogl_graphics_driver::set_upscale_shader(const std::string &name) {
        pending_upscale_shader_ = name;
        active_upscale_shader_ = name;
    }

    void ogl_graphics_driver::bind_swapchain_framebuf() {
        if (surface_update_needed) {
            context_->update_surface(new_surface);
            surface_update_needed = false;
        }

        context_->update();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ogl_graphics_driver::update_surface(void *new_surface_set) {
        new_surface = new_surface_set;
        surface_update_needed = true;
    }

    void ogl_graphics_driver::draw_rectangle(const eka2l1::rect &brush_rect) {
        if (!brush_program) {
            do_init();
        }

        brush_program->use(this);

        // Build model matrix
        glm::mat4 model_matrix = glm::identity<glm::mat4>();
        model_matrix = glm::translate(model_matrix, { brush_rect.top.x, brush_rect.top.y, 0.0f });
        model_matrix = glm::scale(model_matrix, { brush_rect.size.x, brush_rect.size.y, 0.0f });

        glUniformMatrix4fv(model_loc_brush, 1, false, glm::value_ptr(model_matrix));
        glUniformMatrix4fv(proj_loc_brush, 1, false, glm::value_ptr(projection_matrix));

        // Supply brush
        glUniform4fv(color_loc_brush, 1, brush_color.elements.data());

        glBindVertexArray(brush_vao);
        glBindBuffer(GL_ARRAY_BUFFER, brush_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sprite_ibo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glBindVertexArray(0);
    }

    void ogl_graphics_driver::draw_rectangle(command &cmd) {
        eka2l1::rect brush_rect;
        unpack_u64_to_2u32(cmd.data_[0], brush_rect.top.x, brush_rect.top.y);
        unpack_u64_to_2u32(cmd.data_[1], brush_rect.size.x, brush_rect.size.y);

        if (brush_rect.size.x == 0) {
            brush_rect.size.x = current_fb_width;
        }

        if (brush_rect.size.y == 0) {
            brush_rect.size.y = current_fb_height;
        }

        draw_rectangle(brush_rect);
    }

    void ogl_graphics_driver::draw_bitmap(command &cmd) {
        if (!sprite_program) {
            do_init();
        }

        // Get bitmap to draw
        drivers::handle to_draw = static_cast<drivers::handle>(cmd.data_[0]);
        std::uint32_t flags = static_cast<std::uint32_t>(cmd.data_[7] >> 32);

        bitmap *bmp = get_bitmap(to_draw);
        texture *draw_texture = nullptr;

        if (!bmp) {
            draw_texture = reinterpret_cast<texture*>(get_graphics_object(to_draw));

            if (!draw_texture) {
                LOG_ERROR(DRIVER_GRAPHICS, "Invalid bitmap handle to draw");
                return;
            }
        } else {
            draw_texture = bmp->tex.get();
        }

        drivers::handle mask_to_use = static_cast<drivers::handle>(cmd.data_[1]);

        bitmap *mask_bmp = nullptr;
        texture *mask_draw_texture = nullptr;

        if (mask_to_use) {
            mask_bmp = get_bitmap(mask_to_use);

            if (!mask_bmp) {
                mask_draw_texture = reinterpret_cast<texture*>(get_graphics_object(mask_to_use));

                if (!mask_draw_texture) {
                    LOG_ERROR(DRIVER_GRAPHICS, "Mask handle was provided but invalid!");
                    return;
                }
            } else {
                mask_draw_texture = mask_bmp->tex.get();
            }
        }

        eka2l1::rect dest_rect;
        unpack_u64_to_2u32(cmd.data_[2], dest_rect.top.x, dest_rect.top.y);
        unpack_u64_to_2u32(cmd.data_[3], dest_rect.size.x, dest_rect.size.y);

        if (flags & bitmap_draw_flag_use_upscale_shader) {
            commit_upscale_shader_change();
            upscale_program->use(this);
        } else {
            if (mask_bmp) {
                mask_program->use(this);
            } else {
                sprite_program->use(this);
            }
        }

        // Build texcoords
        eka2l1::rect source_rect;
        unpack_u64_to_2u32(cmd.data_[4], source_rect.top.x, source_rect.top.y);
        unpack_u64_to_2u32(cmd.data_[5], source_rect.size.x, source_rect.size.y);

        eka2l1::vec2 origin = eka2l1::vec2(0, 0);
        unpack_u64_to_2u32(cmd.data_[6], origin.x, origin.y);

        std::uint32_t rot_f32 = static_cast<std::uint32_t>(cmd.data_[7]);
        float rotation = *reinterpret_cast<float*>(&rot_f32);

        struct sprite_vertex {
            float top[2];
            float coord[2];
        } verts[4];

        static GLfloat verts_default[] = {
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f
        };

        static GLfloat verts_default_flipped[] = {
            0.0f,
            1.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            0.0f
        };

        void *vert_pointer = verts_default;

        bool need_texture_flip = (flags & bitmap_draw_flag_flip);

        if (!source_rect.empty()) {
            const float texel_width = 1.0f / draw_texture->get_size().x;
            const float texel_height = 1.0f / draw_texture->get_size().y;

            // Bottom left
            verts[0].top[0] = 0.0f;
            verts[0].top[1] = 1.0f;

            verts[0].coord[0] = source_rect.top.x * texel_width;
            verts[0].coord[1] = (source_rect.top.y + source_rect.size.y) * texel_height;

            // Top right
            verts[1].top[0] = 1.0f;
            verts[1].top[1] = 0.0f;

            verts[1].coord[0] = (source_rect.top.x + source_rect.size.x) * texel_width;
            verts[1].coord[1] = source_rect.top.y * texel_height;

            // Top left
            verts[2].top[0] = 0.0f;
            verts[2].top[1] = 0.0f;

            verts[2].coord[0] = source_rect.top.x * texel_width;
            verts[2].coord[1] = source_rect.top.y * texel_height;

            // Bottom right
            verts[3].top[0] = 1.0f;
            verts[3].top[1] = 1.0f;

            verts[3].coord[0] = (source_rect.top.x + source_rect.size.x) * texel_width;
            verts[3].coord[1] = (source_rect.top.y + source_rect.size.y) * texel_height;

            if (need_texture_flip) {
                std::swap(verts[0].coord[1], verts[2].coord[1]);
                std::swap(verts[1].coord[1], verts[3].coord[1]);
            }

            vert_pointer = verts;
        } else if (need_texture_flip) {
            vert_pointer = verts_default_flipped;
        }

        glBindVertexArray(sprite_vao);
        glBindBuffer(GL_ARRAY_BUFFER, sprite_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), nullptr, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), vert_pointer, GL_STATIC_DRAW);

        if (flags & bitmap_draw_flag_use_upscale_shader) {
            glEnableVertexAttribArray(in_position_loc_upscale);
        } else {
            glEnableVertexAttribArray(mask_draw_texture ? in_position_loc_mask : in_position_loc);
        }

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)0);
        
        if (flags & bitmap_draw_flag_use_upscale_shader) {
            glEnableVertexAttribArray(in_position_loc_upscale);
        } else {
            glEnableVertexAttribArray(mask_draw_texture ? in_texcoord_loc_mask : in_texcoord_loc);
        }

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid *)(2 * sizeof(GLfloat)));

        if (mask_draw_texture) {
            glUniform1i(source_loc_mask, 0);
            glUniform1i(mask_loc_mask, 1);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(draw_texture->driver_handle()));

        if (mask_draw_texture) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(mask_draw_texture->driver_handle()));
        }

        if (source_rect.size.x == 0) {
            source_rect.size.x = draw_texture->get_size().x;
        }

        if (source_rect.size.y == 0) {
            source_rect.size.y = draw_texture->get_size().y;
        }

        if (dest_rect.size.x == 0) {
            dest_rect.size.x = source_rect.size.x;
        }

        if (dest_rect.size.y == 0) {
            dest_rect.size.y = source_rect.size.y;
        }

        // Build model matrix
        glm::mat4 model_matrix = glm::identity<glm::mat4>();

        model_matrix = glm::translate(model_matrix, { dest_rect.top.x, dest_rect.top.y, 0.0f });

        model_matrix = glm::translate(model_matrix, glm::vec3(static_cast<float>(origin.x), static_cast<float>(origin.y), 0.0f));
        model_matrix = glm::rotate(model_matrix, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        model_matrix = glm::translate(model_matrix, glm::vec3(static_cast<float>(-origin.x), static_cast<float>(-origin.y), 0.0f));
        model_matrix = glm::scale(model_matrix, glm::vec3(dest_rect.size.x, dest_rect.size.y, 1.0f));

        // Supply brush
        const GLfloat color[] = { 255.0f, 255.0f, 255.0f, 255.0f };

        if (flags & bitmap_draw_flag_use_upscale_shader) {
            glUniformMatrix4fv(model_upscaled_loc, 1, false, glm::value_ptr(model_matrix));
            glUniformMatrix4fv(proj_upscaled_loc, 1, false, glm::value_ptr(projection_matrix));

            glUniform4fv(color_upscaled_loc, 1, color);

            float texel_delta[2] = { 1.0f / static_cast<float>(source_rect.size.x), 1.0f / static_cast<float>(source_rect.size.y) };
            float pixel_delta[2] = { 1.0f / static_cast<float>(dest_rect.size.x), 1.0f / static_cast<float>(dest_rect.size.y) };

            glUniform2fv(texel_delta_upscaled_loc_, 1, texel_delta);
            glUniform2fv(pixel_delta_upscaled_loc_, 1, pixel_delta);
        } else {
            glUniformMatrix4fv((mask_draw_texture ? model_loc_mask : model_loc), 1, false, glm::value_ptr(model_matrix));
            glUniformMatrix4fv((mask_draw_texture ? proj_loc_mask : proj_loc), 1, false, glm::value_ptr(projection_matrix));

            if (flags & bitmap_draw_flag_use_brush) {
                glUniform4fv((mask_draw_texture ? color_loc_mask : color_loc), 1, brush_color.elements.data());
            } else {
                glUniform4fv((mask_draw_texture ? color_loc_mask : color_loc), 1, color);
            }
        }

        if (mask_draw_texture) {
            glUniform1f(invert_loc_mask, (flags & bitmap_draw_flag_invert_mask) ? 1.0f : 0.0f);
            glUniform1f(flat_blend_loc_mask, (flags & bitmap_draw_flag_flat_blending) ? 1.0f : 0.0f);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sprite_ibo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

        glBindVertexArray(0);
    }

    void ogl_graphics_driver::clip_rect(command &cmd) {
        eka2l1::rect clip_rect;
        unpack_u64_to_2u32(cmd.data_[0], clip_rect.top.x, clip_rect.top.y);
        unpack_u64_to_2u32(cmd.data_[1], clip_rect.size.x, clip_rect.size.y);

        if (cmd.opcode_ == drivers::graphics_driver_clip_bitmap_rect) {
            if (binding != nullptr) {
                // Top left translated by shader is bottom left now
                clip_rect.size.y *= -1;
            }
        }

        glScissor(clip_rect.top.x, ((clip_rect.size.y < 0) ? clip_rect.top.y : (current_fb_height - (clip_rect.top.y + clip_rect.size.y))), clip_rect.size.x, common::abs(clip_rect.size.y));
    }

    void ogl_graphics_driver::clip_region(command &cmd) {
        common::region to_clip;
        eka2l1::rect *to_clip_rects = reinterpret_cast<eka2l1::rect*>(cmd.data_[1]);

        to_clip.rects_.insert(to_clip.rects_.begin(), to_clip_rects, to_clip_rects + cmd.data_[0]);

        float scale = 0.0f;
        float temp = 0.0f;

        unpack_to_two_floats(cmd.data_[2], scale, temp);

        delete[] to_clip_rects;

        if (to_clip.empty()) {
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_STENCIL_TEST);

            return;
        }

        if (to_clip.rects_.size() == 1) {
            glEnable(GL_SCISSOR_TEST);
            glDisable(GL_STENCIL_TEST);

            eka2l1::rect clip_rect = to_clip.rects_[0];
            clip_rect.scale(scale);

            glScissor(clip_rect.top.x, ((binding != nullptr) ? clip_rect.top.y : (current_fb_height - (clip_rect.top.y + clip_rect.size.y))),
                clip_rect.size.x, clip_rect.size.y);
            
            return;
        }

        glEnable(GL_STENCIL_TEST);
        glDisable(GL_SCISSOR_TEST);

        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        glStencilFunc(GL_NEVER, 1, 0xFF);
        glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);
        glStencilMask(0xFF);

        for (std::size_t i = 0; i < to_clip.rects_.size(); i++) {
            if (to_clip.rects_[i].valid()) {
                to_clip.rects_[i].scale(scale);
                draw_rectangle(to_clip.rects_[i]);
            }
        }

        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }

    static GLenum prim_mode_to_gl_enum(const graphics_primitive_mode prim_mode) {
        switch (prim_mode) {
        case graphics_primitive_mode::triangles:
            return GL_TRIANGLES;
        
        case graphics_primitive_mode::line_loop:
            return GL_LINE_LOOP;

        case graphics_primitive_mode::line_strip:
            return GL_LINE_STRIP;

        case graphics_primitive_mode::lines:
            return GL_LINES;

        case graphics_primitive_mode::points:
            return GL_POINTS;

        case graphics_primitive_mode::triangle_fan:
            return GL_TRIANGLE_FAN;

        case graphics_primitive_mode::triangle_strip:
            return GL_TRIANGLE_STRIP;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    void ogl_graphics_driver::draw_indexed(command &cmd) {
        graphics_primitive_mode prim_mode = graphics_primitive_mode::triangles;
        int count = 0;
        data_format val_type = data_format::word;
        int index_off = 0;
        int vert_off = static_cast<int>(cmd.data_[2]);

        unpack_u64_to_2u32(cmd.data_[0], prim_mode, count);
        unpack_u64_to_2u32(cmd.data_[1], val_type, index_off);

        std::uint64_t index_off_64 = index_off;

        if (active_input_descriptors_) {
            active_input_descriptors_->activate(vbo_slots_);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_current_);

        if (vert_off == 0) {
            glDrawElements(prim_mode_to_gl_enum(prim_mode), count, data_format_to_gl_enum(val_type), reinterpret_cast<GLvoid *>(index_off_64));
        } else {
            glDrawElementsBaseVertex(prim_mode_to_gl_enum(prim_mode), count, data_format_to_gl_enum(val_type), reinterpret_cast<GLvoid *>(index_off_64), vert_off);
        }
    }

    void ogl_graphics_driver::draw_array(command &cmd) {
        graphics_primitive_mode prim_mode = graphics_primitive_mode::triangles;
        std::int32_t first = 0;
        std::int32_t count = 0;
        std::int32_t instance_count = 0;

        unpack_u64_to_2u32(cmd.data_[0], prim_mode, first);
        unpack_u64_to_2u32(cmd.data_[1], count, instance_count);

        if (active_input_descriptors_) {
            active_input_descriptors_->activate(vbo_slots_);
        }

        if (instance_count == 0) {
            glDrawArrays(prim_mode_to_gl_enum(prim_mode), first, count);
        } else {
            glDrawArraysInstanced(prim_mode_to_gl_enum(prim_mode), first, count, instance_count);
        }
    }

    void ogl_graphics_driver::set_viewport(const eka2l1::rect &viewport) {
        glViewport(viewport.top.x, ((viewport.size.y < 0) ? viewport.top.y : (current_fb_height - (viewport.top.y + viewport.size.y))), viewport.size.x, common::abs(viewport.size.y));
    }

    void ogl_graphics_driver::set_feature(command &cmd) {
        drivers::graphics_feature feature;
        bool enable = true;

        unpack_u64_to_2u32(cmd.data_[0], feature, enable);

        GLenum translated_feature;
        switch (feature) {
        case drivers::graphics_feature::blend:
            translated_feature = GL_BLEND;
            break;

        case drivers::graphics_feature::clipping:
            translated_feature = GL_SCISSOR_TEST;
            break;

        case drivers::graphics_feature::cull:
            translated_feature = GL_CULL_FACE;
            break;

        case drivers::graphics_feature::depth_test:
            translated_feature = GL_DEPTH_TEST;
            break;

        case drivers::graphics_feature::stencil_test:
            translated_feature = GL_STENCIL_TEST;
            break;

        case drivers::graphics_feature::sample_coverage:
            translated_feature = GL_SAMPLE_COVERAGE;
            break;

        case drivers::graphics_feature::sample_alpha_to_one:
            if (is_gles)
                return;

            translated_feature = GL_SAMPLE_ALPHA_TO_ONE;
            break;

        case drivers::graphics_feature::sample_alpha_to_coverage:
            translated_feature = GL_SAMPLE_ALPHA_TO_COVERAGE;
            break;

        case drivers::graphics_feature::polygon_offset_fill:
            translated_feature = GL_POLYGON_OFFSET_FILL;
            break;

        case drivers::graphics_feature::line_smooth:
            if (is_gles)
                return;

            translated_feature = GL_LINE_SMOOTH;
            break;

        case drivers::graphics_feature::multisample:
            if (is_gles)
                return;

            translated_feature = GL_MULTISAMPLE;
            break;

        case drivers::graphics_feature::dither:
            translated_feature = GL_DITHER;
            break;

        default:
            return;
        }

        if (enable) {
            glEnable(translated_feature);
        } else {
            glDisable(translated_feature);
        }
    }

    void ogl_graphics_driver::set_viewport(command &cmd) {
        eka2l1::rect viewport;
        unpack_u64_to_2u32(cmd.data_[0], viewport.top.x, viewport.top.y);
        unpack_u64_to_2u32(cmd.data_[1], viewport.size.x, viewport.size.y);

        if (cmd.opcode_ == drivers::graphics_driver_clip_bitmap_rect) {
            if (binding != nullptr) {
                // Top left translated by shader is bottom left now
                viewport.size.y *= -1;
            }
        }

        set_viewport(viewport);
    }

    static GLenum blend_equation_to_gl_enum(const blend_equation eq) {
        switch (eq) {
        case blend_equation::add:
            return GL_FUNC_ADD;

        case blend_equation::sub:
            return GL_FUNC_SUBTRACT;

        case blend_equation::isub:
            return GL_FUNC_REVERSE_SUBTRACT;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    static GLenum blend_factor_to_gl_enum(const blend_factor fac) {
        switch (fac) {
        case blend_factor::one:
            return GL_ONE;

        case blend_factor::zero:
            return GL_ZERO;

        case blend_factor::frag_out_alpha:
            return GL_SRC_ALPHA;

        case blend_factor::one_minus_frag_out_alpha:
            return GL_ONE_MINUS_SRC_ALPHA;

        case blend_factor::current_alpha:
            return GL_DST_ALPHA;

        case blend_factor::one_minus_current_alpha:
            return GL_ONE_MINUS_DST_ALPHA;

        case blend_factor::frag_out_color:
            return GL_SRC_COLOR;

        case blend_factor::one_minus_frag_out_color:
            return GL_ONE_MINUS_SRC_COLOR;

        case blend_factor::current_color:
            return GL_DST_COLOR;

        case blend_factor::one_minus_current_color:
            return GL_ONE_MINUS_DST_COLOR;

        case blend_factor::frag_out_alpha_saturate:
            return GL_SRC_ALPHA_SATURATE;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    void ogl_graphics_driver::blend_formula(command &cmd) {
        blend_equation rgb_equation = blend_equation::add;
        blend_equation a_equation = blend_equation::add;
        blend_factor rgb_frag_out_factor = blend_factor::one;
        blend_factor rgb_current_factor = blend_factor::zero;
        blend_factor a_frag_out_factor = blend_factor::one;
        blend_factor a_current_factor = blend_factor::zero;

        unpack_u64_to_2u32(cmd.data_[0], rgb_equation, a_equation);
        unpack_u64_to_2u32(cmd.data_[1], rgb_frag_out_factor, rgb_current_factor);
        unpack_u64_to_2u32(cmd.data_[2], a_frag_out_factor, a_current_factor);

        glBlendEquationSeparate(blend_equation_to_gl_enum(rgb_equation), blend_equation_to_gl_enum(a_equation));
        glBlendFuncSeparate(blend_factor_to_gl_enum(rgb_frag_out_factor), blend_factor_to_gl_enum(rgb_current_factor),
            blend_factor_to_gl_enum(a_frag_out_factor), blend_factor_to_gl_enum(a_current_factor));
    }

    static const GLint rendering_face_to_gl_enum(const rendering_face face) {
        switch (face) {
        case rendering_face::back:
            return GL_BACK;

        case rendering_face::front:
            return GL_FRONT;

        case rendering_face::back_and_front:
            return GL_FRONT_AND_BACK;

        default:
            break;
        }

        return GL_FRONT_AND_BACK;
    }

    static const GLint stencil_action_to_gl_enum(const stencil_action action) {
        switch (action) {
        case stencil_action::keep:
            return GL_KEEP;

        case stencil_action::replace:
            return GL_REPLACE;

        case stencil_action::set_to_zero:
            return GL_ZERO;

        case stencil_action::invert:
            return GL_INVERT;

        case stencil_action::decrement:
            return GL_DECR;

        case stencil_action::decrement_wrap:
            return GL_DECR_WRAP;

        case stencil_action::increment:
            return GL_INCR;

        case stencil_action::increment_wrap:
            return GL_INCR_WRAP;

        default:
            break;
        }

        return GL_KEEP;
    }

    void ogl_graphics_driver::set_stencil_action(command &cmd) {
        rendering_face face_to_operate = rendering_face::back_and_front;
        stencil_action on_stencil_fail = stencil_action::keep;
        stencil_action on_stencil_pass_depth_fail = stencil_action::keep;
        stencil_action on_stencil_depth_pass = stencil_action::replace;

        unpack_u64_to_2u32(cmd.data_[0], face_to_operate, on_stencil_fail);
        unpack_u64_to_2u32(cmd.data_[1], on_stencil_pass_depth_fail, on_stencil_depth_pass);

        glStencilOpSeparate(rendering_face_to_gl_enum(face_to_operate), stencil_action_to_gl_enum(on_stencil_fail),
            stencil_action_to_gl_enum(on_stencil_pass_depth_fail), stencil_action_to_gl_enum(on_stencil_depth_pass));
    }

    static const GLint condition_func_to_gl_enum(const condition_func func) {
        switch (func) {
        case condition_func::never:
            return GL_NEVER;

        case condition_func::always:
            return GL_ALWAYS;

        case condition_func::equal:
            return GL_EQUAL;

        case condition_func::not_equal:
            return GL_NOTEQUAL;

        case condition_func::less:
            return GL_LESS;

        case condition_func::less_or_equal:
            return GL_LEQUAL;

        case condition_func::greater:
            return GL_GREATER;

        case condition_func::greater_or_equal:
            return GL_GEQUAL;

        default:
            break;
        }

        return GL_NEVER;
    }

    void ogl_graphics_driver::set_stencil_pass_condition(command &cmd) {
        condition_func pass_func = condition_func::always;
        rendering_face face_to_operate = rendering_face::back_and_front;
        std::int32_t ref_value = 0;
        std::uint32_t mask = 0xFF;

        unpack_u64_to_2u32(cmd.data_[0], face_to_operate, pass_func);
        unpack_u64_to_2u32(cmd.data_[1], ref_value, mask);

        glStencilFuncSeparate(rendering_face_to_gl_enum(face_to_operate), condition_func_to_gl_enum(pass_func),
            ref_value, mask);
    }

    void ogl_graphics_driver::set_stencil_mask(command &cmd) {
        rendering_face face_to_operate = rendering_face::back_and_front;
        std::uint32_t mask = 0xFF;

        unpack_u64_to_2u32(cmd.data_[0], face_to_operate, mask);
        glStencilMaskSeparate(rendering_face_to_gl_enum(face_to_operate), mask);
    }

    void ogl_graphics_driver::set_depth_mask(command &cmd) {
        std::uint32_t mask = static_cast<std::uint32_t>(cmd.data_[0]);
        glDepthMask(mask);
    }

    void ogl_graphics_driver::clear(command &cmd) {
        float color_to_clear[6];
        std::uint8_t clear_bits = static_cast<std::uint8_t>(cmd.data_[3]);

        unpack_to_two_floats(cmd.data_[0], color_to_clear[0], color_to_clear[1]);
        unpack_to_two_floats(cmd.data_[1], color_to_clear[2], color_to_clear[3]);
        unpack_to_two_floats(cmd.data_[2], color_to_clear[4], color_to_clear[5]);

        std::uint32_t gl_flags = 0;

        if (clear_bits & draw_buffer_bit_color_buffer) {
            glClearColor(color_to_clear[0], color_to_clear[1], color_to_clear[2], color_to_clear[3]);
            gl_flags |= GL_COLOR_BUFFER_BIT;
        }

        if (clear_bits & draw_buffer_bit_depth_buffer) {
#ifdef EKA2L1_PLATFORM_ANDROID
            glClearDepthf(color_to_clear[4]);
#else
            glClearDepth(color_to_clear[4]);
#endif
            gl_flags |= GL_DEPTH_BUFFER_BIT;
        }

        if (clear_bits & draw_buffer_bit_stencil_buffer) {
            glClearStencil(static_cast<GLint>(color_to_clear[5] * 255.0f));
            gl_flags |= GL_STENCIL_BUFFER_BIT;
        }

        glClear(gl_flags);
    }

    void ogl_graphics_driver::set_point_size(command &cmd) {
        std::uint8_t to_set_point_size = static_cast<std::uint8_t>(cmd.data_[0]);
        point_size = static_cast<float>(to_set_point_size);
    }

    void ogl_graphics_driver::set_pen_style(command &cmd) {
        pen_style to_set_style = static_cast<pen_style>(cmd.data_[0]);
        line_style = to_set_style;
    }

    void ogl_graphics_driver::prepare_draw_lines_shared() {
        std::uint32_t bit_pattern = 0;

        // Taken from techwinder's code in the following link below. Thank you!
        // https://stackoverflow.com/questions/6017176/gllinestipple-deprecated-in-opengl-3-1
        switch (line_style) {
        case pen_style_solid:
            bit_pattern = 0xFFFF;
            break;

        case pen_style_dotted:
            bit_pattern = 0x6666;
            break;

        case pen_style_dashed:
            bit_pattern = 0x3F3F;
            break;
        
        case pen_style_dashed_dot:
            bit_pattern = 0xFF18;
            break;

        case pen_style_dashed_dot_dot:
            bit_pattern = 0x7E66;
            break;

        default:
            LOG_WARN(DRIVER_GRAPHICS, "Unrecognised pen style {}!", static_cast<int>(line_style));
            return;
        }

        pen_program->use(this);

        glUniform1f(point_size_loc_pen, point_size);
        glUniform1ui(pattern_bytes_loc_pen, bit_pattern);
        
        glm::mat4 model_matrix = glm::identity<glm::mat4>();

        glUniformMatrix4fv(model_loc_pen, 1, false, glm::value_ptr(model_matrix));
        glUniformMatrix4fv(proj_loc_pen, 1, false, glm::value_ptr(projection_matrix));
        glUniform4fv(color_loc_pen, 1, brush_color.elements.data());

        GLfloat viewport[2] = { static_cast<GLfloat>(current_fb_width), static_cast<GLfloat>(current_fb_height) };
        glUniform2fv(viewport_loc_pen, 1, viewport);
    }

    void ogl_graphics_driver::draw_line(command &cmd) {
        if (line_style == pen_style_none) {
            return;
        }

        eka2l1::point start;
        eka2l1::point end;
        
        unpack_u64_to_2u32(cmd.data_[0], start.x, start.y);
        unpack_u64_to_2u32(cmd.data_[1], end.x, end.y);

        prepare_draw_lines_shared();

        GLfloat data_send_array[] = {
            static_cast<float>(start.x), static_cast<float>(start.y),
            static_cast<float>(end.x), static_cast<float>(end.y)
        };

        glBindVertexArray(pen_vao);
        glBindBuffer(GL_ARRAY_BUFFER, pen_vbo);
        glEnableVertexAttribArray(0);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data_send_array), nullptr, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data_send_array), data_send_array, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);

        glDrawArrays(GL_LINES, 0, 2);
    }

    void ogl_graphics_driver::draw_polygon(command &cmd) {
        if (line_style == pen_style_none) {
            return;
        }

        prepare_draw_lines_shared();
        
        std::size_t point_count = static_cast<std::size_t>(cmd.data_[0]);
        eka2l1::point *point_list = reinterpret_cast<eka2l1::point*>(cmd.data_[1]);

        glBindVertexArray(pen_vao);
        glBindBuffer(GL_ARRAY_BUFFER, pen_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 8, (GLvoid *)0);
        glBufferData(GL_ARRAY_BUFFER, point_count * 8, nullptr, GL_STATIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, point_count * 8, point_list, GL_STATIC_DRAW);

        // Generate indices for force primitive restart
        // Don't try to recreate buffer (note that nullptr upper is a method of fast clearing)
        std::vector<std::uint32_t> indicies((point_count - 1) * 2);
        for (std::size_t i = 0; i < point_count - 1; i++) {
            indicies[i * 2] = static_cast<std::uint32_t>(i);
            indicies[i * 2 + 1] = static_cast<std::uint32_t>(i + 1);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pen_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicies.size() * sizeof(int), indicies.data(), GL_STATIC_DRAW);

        glDrawElements(GL_LINES, static_cast<GLsizei>(indicies.size()), GL_UNSIGNED_INT, 0);

        delete[] point_list;
    }

    void ogl_graphics_driver::set_cull_face(command &cmd) {
        rendering_face face_to_cull = static_cast<rendering_face>(cmd.data_[0]);

        switch (face_to_cull) {
        case rendering_face::back:
            glCullFace(GL_BACK);
            break;

        case rendering_face::front:
            glCullFace(GL_FRONT);
            break;

        case rendering_face::back_and_front:
            glCullFace(GL_FRONT_AND_BACK);
            break;

        default:
            break;
        }
    }

    void ogl_graphics_driver::set_depth_func(command &cmd) {
        condition_func func = static_cast<condition_func>(cmd.data_[0]);
        glDepthFunc(condition_func_to_gl_enum(func));
    }

    void ogl_graphics_driver::set_front_face_rule(command &cmd) {
        rendering_face_determine_rule rule = static_cast<rendering_face_determine_rule>(cmd.data_[0]);
        if (rule == rendering_face_determine_rule::vertices_counter_clockwise) {
            glFrontFace(GL_CCW);
        } else {
            glFrontFace(GL_CW);
        }
    }

    void ogl_graphics_driver::set_color_mask(command &cmd) {
        std::uint64_t dat = cmd.data_[0];
        glColorMask(dat & 1, dat & 2, dat & 4, dat & 8);
    }

    void ogl_graphics_driver::set_uniform(command &cmd) {
        drivers::shader_var_type var_type;
        std::uint8_t *data = reinterpret_cast<std::uint8_t*>(cmd.data_[1]);
        int binding = 0;

        unpack_u64_to_2u32(cmd.data_[0], binding, var_type);

        switch (var_type) {
        case shader_var_type::integer: {
            glUniform1iv(binding, static_cast<GLsizei>((cmd.data_[2] + 3) / 4), reinterpret_cast<const GLint *>(data));
            delete[] data;

            return;
        }

        case shader_var_type::real:
            glUniform1fv(binding, static_cast<GLsizei>((cmd.data_[2] + 3) / 4), reinterpret_cast<const GLfloat*>(data));
            delete[] data;

            return;

        case shader_var_type::mat2: {
            glUniformMatrix2fv(binding, static_cast<GLsizei>((cmd.data_[2] + 15) / 16), GL_FALSE, reinterpret_cast<const GLfloat *>(data));
            delete[] data;

            return;
        }

        case shader_var_type::mat3: {
            glUniformMatrix2fv(binding, static_cast<GLsizei>((cmd.data_[2] + 35) / 36), GL_FALSE, reinterpret_cast<const GLfloat *>(data));
            delete[] data;

            return;
        }

        case shader_var_type::mat4: {
            glUniformMatrix4fv(binding, static_cast<GLsizei>((cmd.data_[2] + 63) / 64), GL_FALSE, reinterpret_cast<const GLfloat *>(data));
            delete[] data;

            return;
        }

        case shader_var_type::vec3: {
            glUniform3fv(binding, static_cast<GLsizei>((cmd.data_[2] + 11) / 12), reinterpret_cast<const GLfloat *>(data));
            delete[] data;

            return;
        }

        case shader_var_type::vec4: {
            glUniform4fv(binding, static_cast<GLsizei>((cmd.data_[2] + 15) / 16), reinterpret_cast<const GLfloat *>(data));
            delete[] data;

            return;
        }

        default:
            break;
        }

        LOG_ERROR(DRIVER_GRAPHICS, "Unable to set GL uniform!");
    }

    void ogl_graphics_driver::set_texture_for_shader(command &cmd) {
        std::int32_t texture_slot = 0;
        std::int32_t shader_binding = 0;
        drivers::shader_module_type mtype = static_cast<drivers::shader_module_type>(cmd.data_[1]);

        unpack_u64_to_2u32(cmd.data_[0], texture_slot, shader_binding);
        glUniform1i(shader_binding, texture_slot);
    }

    void ogl_graphics_driver::bind_vertex_buffers(command &cmd) {
        drivers::handle *arr = reinterpret_cast<drivers::handle*>(cmd.data_[0]);
        std::uint32_t starting_slots = 0;
        std::uint32_t count = 0;

        unpack_u64_to_2u32(cmd.data_[1], starting_slots, count);

        if (starting_slots + count >= GL_BACKEND_MAX_VBO_SLOTS) {
            LOG_ERROR(DRIVER_GRAPHICS, "Slot to bind VBO exceed maximum (startSlot={}, count={})", starting_slots, count);
            delete[] arr;

            return;
        }

        for (std::uint32_t i = 0; i < count; i++) {
            ogl_buffer *bufobj = reinterpret_cast<ogl_buffer *>(get_graphics_object(arr[i]));

            if (!bufobj) {
                continue;
            }

            vbo_slots_[starting_slots + i] = bufobj->buffer_handle();
        }

        delete[] arr;
    }

    void ogl_graphics_driver::bind_index_buffer(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
        ogl_buffer *bufobj = reinterpret_cast<ogl_buffer *>(get_graphics_object(h));
        if (!bufobj) {
            return;
        }

        index_buffer_current_ = bufobj->buffer_handle();
    }

    void ogl_graphics_driver::bind_input_descriptors(command &cmd) {
        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);
        input_descriptors_ogl *descs = reinterpret_cast<input_descriptors_ogl *>(get_graphics_object(h));
        if (!descs) {
            return;
        }

        active_input_descriptors_ = descs;
    }

    void ogl_graphics_driver::set_line_width(command &cmd) {
        if (!support_line_width_) {
            return;
        }

        std::uint32_t w_32 = static_cast<std::uint32_t>(cmd.data_[0]);
        float w = *reinterpret_cast<float*>(&w_32);

        GLint range[2];
        glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, range);

        if (range[1] >= w) {
            glad_glLineWidth(w);

            if (glGetError() == GL_INVALID_VALUE) {
                LOG_INFO(DRIVER_GRAPHICS, "OpenGL driver does not support line width feature. Disabled from now on.");
                support_line_width_ = false;
            }
        }
    }

    void ogl_graphics_driver::set_depth_bias(command &cmd) {
        float constant_factor = 0.0f;
        float clamp = 0.0f;
        float slope_factor = 0.0f;
        float temp = 0;

        unpack_to_two_floats(cmd.data_[0], constant_factor, slope_factor);
        unpack_to_two_floats(cmd.data_[1], clamp, temp);

        glPolygonOffset(slope_factor, constant_factor);
    }

    void ogl_graphics_driver::set_depth_range(command &cmd) {
        float min = 0.0f;
        float max = 1.0f;

        unpack_to_two_floats(cmd.data_[0], min, max);

        if (is_gles) {
            glDepthRangef(min, max);
        } else {
            glDepthRange(static_cast<double>(min), static_cast<double>(max));
        }
    }

    void ogl_graphics_driver::set_texture_anisotrophy(command &cmd) {
        if ((feature_flags_ & OGL_FEATURE_SUPPORT_ANISOTROPHY) == 0) {
            return;
        }

        drivers::handle h = static_cast<drivers::handle>(cmd.data_[0]);

        float max_ani, temp = 0.0f;
        unpack_to_two_floats(cmd.data_[1], max_ani, temp);

        texture *texobj = nullptr;

        if (h & HANDLE_BITMAP) {
            // Bind bitmap as texture
            bitmap *b = get_bitmap(h);

            if (!b) {
                return;
            }

            texobj = b->tex.get();
        } else {
            texobj = reinterpret_cast<texture *>(get_graphics_object(h));
        }

        if (!texobj) {
            return;
        }

        texobj->bind(nullptr, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_ani);
        texobj->unbind(nullptr);
    }
    
    void ogl_graphics_driver::bind_framebuffer(command &cmd) {
        drivers::handle h = cmd.data_[0];
        drivers::framebuffer_bind_type bind_type = static_cast<drivers::framebuffer_bind_type>(cmd.data_[1]);

        if (h == 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        drivers::framebuffer *fb = reinterpret_cast<drivers::framebuffer*>(get_graphics_object(h));
        if (!fb) {
            return;
        }

        fb->bind(this, bind_type);
    }

    void ogl_graphics_driver::save_gl_state() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &backup.last_program);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &backup.last_texture);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &backup.last_active_texture);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &backup.last_array_buffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &backup.last_element_array_buffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &backup.last_vertex_array);
        glGetIntegerv(GL_BLEND_SRC_RGB, &backup.last_blend_src_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &backup.last_blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_RGB, &backup.last_blend_dst_rgb);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &backup.last_blend_dst_alpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &backup.last_blend_equation_rgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &backup.last_blend_equation_alpha);
        glGetIntegerv(GL_VIEWPORT, backup.last_viewport);
        glGetIntegerv(GL_SCISSOR_BOX, backup.last_scissor);
        backup.last_enable_blend = glIsEnabled(GL_BLEND);
        backup.last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        backup.last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        backup.last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    }

    void ogl_graphics_driver::load_gl_state() {
        glUseProgram(backup.last_program);
        glActiveTexture(static_cast<GLenum>(backup.last_active_texture));
        glBindTexture(GL_TEXTURE_2D, backup.last_texture);
        glBindVertexArray(backup.last_vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, backup.last_array_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backup.last_element_array_buffer);

        glBlendEquationSeparate(static_cast<GLenum>(backup.last_blend_equation_rgb),
            static_cast<GLenum>(backup.last_blend_equation_alpha));

        glBlendFuncSeparate(static_cast<GLenum>(backup.last_blend_src_rgb),
            static_cast<GLenum>(backup.last_blend_dst_rgb),
            static_cast<GLenum>(backup.last_blend_src_alpha),
            static_cast<GLenum>(backup.last_blend_dst_alpha));

        if (backup.last_enable_blend == GL_TRUE) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        if (backup.last_enable_cull_face == GL_TRUE) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }

        if (backup.last_enable_depth_test == GL_TRUE) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        if (backup.last_enable_scissor_test == GL_TRUE) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        glViewport(backup.last_viewport[0],
            backup.last_viewport[1],
            static_cast<GLsizei>(backup.last_viewport[2]),
            static_cast<GLsizei>(backup.last_viewport[3]));

        glScissor(backup.last_scissor[0],
            backup.last_scissor[1],
            backup.last_scissor[2],
            backup.last_scissor[3]);
    }

    void ogl_graphics_driver::submit_command_list(command_list &list) {
        if ((list.size_ == 0) || !list.base_ || should_stop) {
            if (list.base_) {
                delete[] list.base_;
            }

            return;
        }
        list_queue.push(list);
    }

    void ogl_graphics_driver::display(command &cmd) {
        context_->swap_buffers();

        disp_hook_();
        finish(cmd.status_, 0);
    }

    void ogl_graphics_driver::dispatch(command &cmd) {
        switch (cmd.opcode_) {
        case graphics_driver_draw_bitmap: {
            draw_bitmap(cmd);
            break;
        }

        case graphics_driver_clip_rect:
        case graphics_driver_clip_bitmap_rect: {
            clip_rect(cmd);
            break;
        }

        case graphics_driver_draw_indexed: {
            draw_indexed(cmd);
            break;
        }

        case graphics_driver_draw_array:
            draw_array(cmd);
            break;

        case graphics_driver_backup_state: {
            save_gl_state();
            break;
        }

        case graphics_driver_restore_state: {
            load_gl_state();
            break;
        }

        case graphics_driver_blend_formula: {
            blend_formula(cmd);
            break;
        }

        case graphics_driver_set_feature: {
            set_feature(cmd);
            break;
        }

        case graphics_driver_stencil_set_action:
            set_stencil_action(cmd);
            break;

        case graphics_driver_stencil_pass_condition:
            set_stencil_pass_condition(cmd);
            break;

        case graphics_driver_stencil_set_mask:
            set_stencil_mask(cmd);
            break;

        case graphics_driver_clear: {
            clear(cmd);
            break;
        }

        case graphics_driver_set_viewport:
        case graphics_driver_set_bitmap_viewport: {
            set_viewport(cmd);
            break;
        }

        case graphics_driver_display: {
            display(cmd);
            break;
        }

        case graphics_driver_draw_rectangle: {
            draw_rectangle(cmd);
            break;
        }

        case graphics_driver_draw_line:
            draw_line(cmd);
            break;

        case graphics_driver_draw_polygon:
            draw_polygon(cmd);
            break;

        case graphics_driver_set_point_size:
            set_point_size(cmd);
            break;

        case graphics_driver_set_pen_style:
            set_pen_style(cmd);
            break;

        case graphics_driver_cull_face:
            set_cull_face(cmd);
            break;

        case graphics_driver_set_front_face_rule:
            set_front_face_rule(cmd);
            break;

        case graphics_driver_set_color_mask:
            set_color_mask(cmd);
            break;

        case graphics_driver_set_depth_func:
            set_depth_func(cmd);
            break;

        case graphics_driver_set_texture_for_shader:
            set_texture_for_shader(cmd);
            break;

        case graphics_driver_set_uniform:
            set_uniform(cmd);
            break;

        case graphics_driver_depth_set_mask:
            set_depth_mask(cmd);
            break;

        case graphics_driver_bind_vertex_buffers:
            bind_vertex_buffers(cmd);
            break;

        case graphics_driver_bind_index_buffer:
            bind_index_buffer(cmd);
            break;

        case graphics_driver_bind_input_descriptor:
            bind_input_descriptors(cmd);
            break;

        case graphics_driver_set_line_width:
            set_line_width(cmd);
            break;

        case graphics_driver_set_depth_bias:
            set_depth_bias(cmd);
            break;

        case graphics_driver_set_depth_range:
            set_depth_range(cmd);
            break;

        case graphics_driver_clip_region:
            clip_region(cmd);
            break;

        case graphics_driver_set_texture_anisotrophy:
            set_texture_anisotrophy(cmd);
            break;

        case graphics_driver_bind_framebuffer:
            bind_framebuffer(cmd);
            break;

        default:
            shared_graphics_driver::dispatch(cmd);
            break;
        }
    }

    void ogl_graphics_driver::run() {
        while (!should_stop) {
            std::optional<command_list> list = list_queue.pop();

            if (!list) {
                LOG_ERROR(DRIVER_GRAPHICS, "Corrupted graphics command list! Emulation halt.");
                break;
            }

            for (std::size_t i = 0; i < list->size_; i++) {
                dispatch(list->base_[i]);
            }

            delete[] list->base_;
        }
    }

    void ogl_graphics_driver::abort() {
        list_queue.abort();
        should_stop = true;

        cond_.notify_all();
    }

    void ogl_graphics_driver::wait_for(int *status) {
        if (should_stop) {
            return;
        }

        driver::wait_for(status);
    }

    std::string ogl_graphics_driver::get_active_upscale_shader() const {
        return active_upscale_shader_;
    }
}
