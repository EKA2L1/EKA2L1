/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#pragma once

#include <dispatch/libraries/gles_shared/def.h>
#include <drivers/graphics/graphics.h>
#include <common/vecx.h>

#include <queue>
#include <string>
#include <vector>

namespace eka2l1 {
    class system;

    namespace drivers {
        class graphics_driver;
    }
}

namespace eka2l1::dispatch {
    struct gles_program_object;

    struct gles_shader_object: public gles_driver_object {
    private:
        std::string source_;
        std::string compile_info_;
        drivers::shader_module_type module_type_;
        bool compile_ok_;
        bool delete_pending_;
        bool source_changed_;

        std::vector<gles_program_object*> attached_programs_;
        void cleanup_current_driver_module();

    public:
        explicit gles_shader_object(egl_context_es_shared &ctx, const drivers::shader_module_type module_type);
        ~gles_shader_object() override;

        void compile(drivers::graphics_driver *drv);
        void set_source(const std::string &source);
        void attach_to(gles_program_object *program);
        void detach_from(gles_program_object *program);
        void delete_object();

        const std::string &get_source() const {
            return source_;
        }

        const std::string get_compile_info() const {
            return compile_info_;
        }

        const std::uint32_t get_source_length() const {
            return static_cast<std::uint32_t>(source_.length());
        }

        const std::uint32_t get_compile_info_length() const {
            return static_cast<std::uint32_t>(compile_info_.length());
        }

        const drivers::shader_module_type get_shader_module_type() const {
            return module_type_;
        }

        const bool is_last_compile_ok() const {
            return compile_ok_;
        }

        const bool is_delete_pending() const {
            return delete_pending_;
        }
    };

    struct gles_program_object: public gles_driver_object {
    private:
        gles_shader_object *attached_vertex_shader_;
        gles_shader_object *attached_fragment_shader_;

        std::string link_log_;
        bool linked_;
        bool one_module_changed_;
        bool delete_pending_;

        drivers::shader_program_metadata metadata_;
        
        void cleanup_current_driver_program();

    public:
        explicit gles_program_object(egl_context_es_shared &ctx);
        ~gles_program_object() override;

        bool attach(gles_shader_object *obj);
        bool detach(gles_shader_object *obj);

        void link(drivers::graphics_driver *drv);

        const std::string &get_link_log() const {
            return link_log_;
        }

        const std::int32_t get_link_log_length() const {
            return static_cast<std::int32_t>(link_log_.length());
        }

        const bool is_linked() const {
            return linked_;
        }

        const bool is_delete_pending() const {
            return delete_pending_;
        }

        void set_delete_pending() {
            delete_pending_ = true;
        }

        std::int32_t attached_shaders_count() const {
            return (attached_vertex_shader_ ? (attached_fragment_shader_ ? 2 : 1) : 0);
        }

        const std::int32_t active_attributes_count() const {
            return linked_ ? metadata_.get_attribute_count() : 0;
        }

        const std::int32_t active_uniforms_count() const {
            return linked_ ? metadata_.get_uniform_count() : 0;
        }

        const std::int32_t active_attribute_max_name_length() const {
            return linked_ ? metadata_.get_attribute_max_name_length() : 0;
        }
        
        const std::int32_t active_uniform_max_name_length() const {
            return linked_ ? metadata_.get_uniform_max_name_length() : 0;
        }
    };

    struct gles_framebuffer_object;

    struct gles_renderbuffer_object : public gles_driver_object {
    private:
        std::uint32_t format_;
        eka2l1::vec2 size_;

        std::vector<gles_framebuffer_object*> attached_fbs_;

    public:
        explicit gles_renderbuffer_object(egl_context_es_shared &ctx);
        ~gles_renderbuffer_object() override;

        std::uint32_t make_storage(drivers::graphics_driver *drv, const eka2l1::vec2 &size, const std::uint32_t format);

        void attach_to(gles_framebuffer_object *obj);
        void detach_from(gles_framebuffer_object *obj);

        const std::uint32_t get_format() const {
            return format_;
        }

        const eka2l1::vec2 get_size() const {
            return size_;
        }
    };

    struct gles_framebuffer_object : public gles_driver_object {
    private:
        gles_driver_object *attached_color_;
        gles_driver_object *attached_depth_;
        gles_driver_object *attached_stencil_;

        bool color_changed_;
        bool depth_changed_;
        bool stencil_changed_;

    public:
        explicit gles_framebuffer_object(egl_context_es_shared &ctx);
        ~gles_framebuffer_object() override;

        void force_detach(gles_driver_object *obj);

        std::uint32_t set_attachment(gles_driver_object *object, const std::uint32_t attachment_type);
        std::uint32_t completed() const;
        std::uint32_t ready_for_draw(drivers::graphics_driver *drv);
    };

    struct egl_context_es2: public egl_context_es_shared {
        std::queue<drivers::handle> compiled_shader_cleanup_;
        std::queue<drivers::handle> linked_program_cleanup_;
        std::stack<drivers::handle> renderbuffer_pool_;
        std::stack<drivers::handle> framebuffer_pool_;

        std::uint32_t binded_renderbuffer_;
        std::uint32_t binded_framebuffer_;

        bool framebuffer_need_reconfigure_;

        explicit egl_context_es2();
        void destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) override;

        egl_context_type context_type() const override {
            return EGL_GLES2_CONTEXT;
        }
    };

    egl_context_es2 *get_es2_active_context(system *sys);
}