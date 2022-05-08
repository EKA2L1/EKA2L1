/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the free software Foundation, either version 3 of the License, or
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

#include <dispatch/libraries/gles_shared/utils.h>
#include <dispatch/libraries/gles2/gles2.h>
#include <dispatch/libraries/gles2/def.h>

#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <system/epoc.h>
#include <services/window/screen.h>
#include <kernel/kernel.h>

namespace eka2l1::dispatch {
    std::string get_es2_extensions(drivers::graphics_driver *driver) {
        std::string original_list = GLES2_STATIC_STRING_EXTENSIONS;
        if (driver->support_extension(drivers::graphics_driver_extension_anisotrophy_filtering)) {
            original_list += "GL_EXT_texture_filter_anisotropic ";
        }
        return original_list;
    }

    egl_context_es2 *get_es2_active_context(system *sys) {
        if (!sys) {
            return nullptr;
        }

        dispatcher *dp = sys->get_dispatcher();
        if (!dp) {
            return nullptr;
        }

        kernel_system *kern = sys->get_kernel_system();
        if (!kern) {
            return nullptr;
        }

        kernel::thread *crr_thread = kern->crr_thread();
        if (!crr_thread) {
            return nullptr;
        }

        dispatch::egl_controller &controller = dp->get_egl_controller();
        kernel::uid crr_thread_uid = crr_thread->unique_id();

        egl_context *context = controller.current_context(crr_thread_uid);
        if (!context || (context->context_type() != EGL_GLES2_CONTEXT)) {
            controller.push_error(crr_thread_uid, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<egl_context_es2*>(context);
    }

    gles_shader_object::gles_shader_object(egl_context_es_shared &ctx, const drivers::shader_module_type module_type)
        : gles_driver_object(ctx)
        , module_type_(module_type)
        , compile_ok_(false)
        , delete_pending_(false)
        , source_changed_(false) {
    }

    void gles_shader_object::compile(drivers::graphics_driver *drv) {
        if (!source_changed_) {
            // Don't waste time compile, when nothing has really changed!
            return;
        }

        std::string changed_source = source_;

        if (changed_source.substr(0, 8) == "#version") {
            std::size_t pos = changed_source.find_first_of('\n');
            if (pos == std::string::npos) {
                compile_ok_ = false;
                compile_info_ = "ERROR: Shader has empty content!";
                source_changed_ = false;

                return;
            }
            changed_source.erase(changed_source.begin(), changed_source.begin() + pos + 1);
        }

        // Add version
        if (drv->is_stricted()) {
            changed_source.insert(0, "#version 100\n");
        } else {
            changed_source.insert(0, "#version 120\n");
        }

        cleanup_current_driver_module();
        driver_handle_ = drivers::create_shader_module(drv, changed_source.data(), changed_source.length(), module_type_, &compile_info_);

        if (!driver_handle_) {
            compile_ok_ = false;
        } else {
            compile_ok_ = true;
        }

        source_changed_ = false;
    }

    void gles_shader_object::cleanup_current_driver_module() {
        if (driver_handle_) {
            egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
            es2_ctx.compiled_shader_cleanup_.push(driver_handle_);
        }
    }

    void gles_shader_object::set_source(const std::string &source) {
        if (source != source_) {
            source_ = source;
            source_changed_ = true;
        }
    }
    
    void gles_shader_object::attach_to(gles_program_object *program) {
        if (std::find(attached_programs_.begin(), attached_programs_.end(), program) == attached_programs_.end()) {
            attached_programs_.push_back(program);
        }
    }
    
    void gles_shader_object::detach_from(gles_program_object *program) {
        auto ite = std::find(attached_programs_.begin(), attached_programs_.end(), program);
        if (ite != attached_programs_.end()) {
            attached_programs_.erase(ite);
        }

        if (attached_programs_.empty() && delete_pending_) {
            delete_pending_ = false;

            egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
            es2_ctx.objects_.remove(client_handle_);
        }
    }

    void gles_program_object::cleanup_current_driver_program() {
        if (driver_handle_) {
            egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
            es2_ctx.linked_program_cleanup_.push(driver_handle_);
        }

        dirty_uniform_locations_.clear();
        uniform_datas_.clear();
    }
    
    void gles_shader_object::delete_object() {
        if (attached_programs_.empty()) {
            egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
            es2_ctx.objects_.remove(client_handle_);
        } else {
            delete_pending_ = true;
        }
    }

    gles_program_object::gles_program_object(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , attached_vertex_shader_(nullptr)
        , attached_fragment_shader_(nullptr)
        , linked_(false)
        , one_module_changed_(false)
        , delete_pending_(false) {
    }

    gles_program_object::~gles_program_object() {
        cleanup_current_driver_program();

        if (attached_vertex_shader_) {
            attached_vertex_shader_->detach_from(this);
        }

        if (attached_fragment_shader_) {
            attached_fragment_shader_->detach_from(this);
        }
    }

    bool gles_program_object::attach(gles_shader_object *obj) {
        if (!obj) {
            return false;
        }

        if (obj->get_shader_module_type() == drivers::shader_module_type::vertex) {
            if (!attached_vertex_shader_) {
                attached_vertex_shader_ = obj;
                one_module_changed_ = true;
                obj->attach_to(this);

                return true;
            }
        } else {
            if (!attached_fragment_shader_) {
                attached_fragment_shader_ = obj;
                one_module_changed_ = true;
                obj->attach_to(this);

                return true;
            }
        }

        return false;
    }

    bool gles_program_object::detach(gles_shader_object *obj) {
        if (!obj) {
            return false;
        }

        if (obj->get_shader_module_type() == drivers::shader_module_type::vertex) {
            if (attached_vertex_shader_ == obj) {
                attached_vertex_shader_ = nullptr;
                one_module_changed_ = true;
                obj->detach_from(this);

                return true;
            }
        } else {
            if (attached_fragment_shader_ == obj) {
                attached_fragment_shader_ = nullptr;
                one_module_changed_ = true;
                obj->detach_from(this);

                return true;
            }
        }

        return false;
    }

    void gles_program_object::link(drivers::graphics_driver *drv) {
        if (!one_module_changed_) {
            return;
        }

        cleanup_current_driver_program();
        linked_ = false;

        if (!attached_fragment_shader_ || !attached_vertex_shader_) {
            return;
        }

        driver_handle_ = drivers::create_shader_program(drv, attached_vertex_shader_->handle_value(), attached_fragment_shader_->handle_value(),
            &metadata_, &link_log_);

        linked_ = (driver_handle_ != 0);
        one_module_changed_ = false;
    }

    std::uint32_t gles_program_object::set_uniform_data(const int binding, const std::uint8_t *data, const std::int32_t data_size,
        const std::int32_t actual_count, drivers::shader_var_type var_type, const std::uint32_t extra_flags) {
        if (!linked_) {
            return GL_INVALID_OPERATION;
        }

        if (actual_count <= 0) {
            return GL_INVALID_VALUE;
        }

        auto cached_ite = uniform_datas_.find(binding);
        if (cached_ite != uniform_datas_.end()) {
            gles_uniform_variable_data_info &cached_data = cached_ite->second;
            if (cached_data.cached_var_type_ != var_type) {
                return GL_INVALID_OPERATION;
            }

            cached_data.data_.resize(data_size);
            cached_data.extra_flags_ = extra_flags;
            std::memcpy(cached_data.data_.data(), data, data_size);
            dirty_uniform_locations_.insert(binding);

            return GL_NO_ERROR;
        }

        std::string name;
        std::int32_t current_iterate_binding = 0;
        std::int32_t current_iterate_array_size = 0;
        drivers::shader_var_type current_iterate_var_type = drivers::shader_var_type::none;

        bool found = false;

        for (std::int32_t i = 0; i < metadata_.get_uniform_count(); i++) {
            metadata_.get_uniform_info(i, name, current_iterate_binding, current_iterate_var_type, current_iterate_array_size);
            if (binding == current_iterate_binding) {
                if ((current_iterate_array_size == 1) && (actual_count > 1)) {
                    return GL_INVALID_OPERATION;
                }
                if ((current_iterate_var_type == drivers::shader_var_type::sampler2d) || (current_iterate_var_type == drivers::shader_var_type::sampler_cube)) {
                    if (var_type != drivers::shader_var_type::integer) {
                        return GL_INVALID_OPERATION;
                    }
                }
                found = true;
                break;
            }
        }

        if (!found) {
            return GL_INVALID_OPERATION;
        }
        
        gles_uniform_variable_data_info info_push;

        info_push.data_.resize(data_size);
        info_push.extra_flags_ = extra_flags;
        info_push.cached_var_type_ = var_type;

        std::memcpy(info_push.data_.data(), data, data_size);
        uniform_datas_.emplace(binding, std::move(info_push));
        dirty_uniform_locations_.insert(binding);

        return GL_NO_ERROR;
    }

    bool gles_program_object::prepare_for_draw() {
        if (!linked_) {
            return false;
        }

        if ((static_cast<egl_context_es2&>(context_)).previous_using_program_ != this) {
            context_.cmd_builder_.use_program(driver_handle_);
        }

        for (int dirty_location: dirty_uniform_locations_) {
            const gles_uniform_variable_data_info &info = uniform_datas_[dirty_location];
            if (info.extra_flags_ & gles_uniform_variable_data_info::EXTRA_FLAG_MATRIX_NEED_TRANSPOSE) {
                // Manual transpose
                std::uint32_t width = 2;

                if (info.cached_var_type_ == drivers::shader_var_type::mat2) {
                    width = 2;
                } else if (info.cached_var_type_ == drivers::shader_var_type::mat3) {
                    width = 3;
                } else {
                    width = 4;
                }

                float data_temp[16]; 
                std::memcpy(data_temp, info.data_.data(), info.data_.size());

                for (std::uint32_t i = 0; i < width; i++) {
                    for (std::uint32_t j = i + 1; j < width; j++) {
                        std::swap(data_temp[i * width + j], data_temp[j * width + i]);
                    }
                }

                context_.cmd_builder_.set_dynamic_uniform(dirty_location, info.cached_var_type_, data_temp, info.data_.size());
            } else {
                context_.cmd_builder_.set_dynamic_uniform(dirty_location, info.cached_var_type_, info.data_.data(), info.data_.size());
            }
        }

        dirty_uniform_locations_.clear();
        return true;
    }

    gles_shader_object::~gles_shader_object() {
        cleanup_current_driver_module();
    }
    
    gles_renderbuffer_object::gles_renderbuffer_object(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , format_(0)
        , current_scale_(0.0f) {
    }

    gles_renderbuffer_object::~gles_renderbuffer_object() {
        for (std::size_t i = 0; i < attached_fbs_.size(); i++) {
            attached_fbs_[i]->force_detach(this);
        }
        if (driver_handle_) {
            egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
            es2_ctx.renderbuffer_pool_.push(driver_handle_);
        }
    }

    std::uint32_t gles_renderbuffer_object::make_storage(drivers::graphics_driver *drv, const eka2l1::vec2 &size, const std::uint32_t format) {
        if ((format != GL_RGBA4_EMU) && (format != GL_RGB5_A1_EMU) && (format != GL_RGB565_EMU) && (format != GL_DEPTH_COMPONENT16_EMU)
            && (format != GL_STENCIL_INDEX8_EMU) && (format != GL_DEPTH24_STENCIL8_OES)) {
            return GL_INVALID_ENUM;
        }

        drivers::texture_format format_driver;
        switch (format) {
        case GL_RGBA4_EMU:
            format_driver = drivers::texture_format::rgba4;
            break;

        case GL_RGB5_A1_EMU:
            format_driver = drivers::texture_format::rgb5_a1;
            break;

        case GL_RGB565_EMU:
            format_driver = drivers::texture_format::rgb565;
            break;

        case GL_DEPTH_COMPONENT16_EMU:
            format_driver = drivers::texture_format::depth16;
            break;

        case GL_STENCIL_INDEX8_EMU:
            format_driver = drivers::texture_format::stencil8;
            break;

        case GL_DEPTH24_STENCIL8_OES:
            format_driver = drivers::texture_format::depth24_stencil8;
            break;

        case GL_RGBA_EMU:
            format_driver = drivers::texture_format::rgba;
            break;

        case GL_RGB_EMU:
            format_driver = drivers::texture_format::rgb;
            break;

        default:
            return GL_INVALID_ENUM;
        }

        format_ = format;
        size_ = size;

        egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
        bool need_recreate = true;

        current_scale_ = context_.draw_surface_->backed_screen_->display_scale_factor;

        if (!driver_handle_) {
            if (!es2_ctx.renderbuffer_pool_.empty()) {
                driver_handle_ = es2_ctx.renderbuffer_pool_.top();
                es2_ctx.renderbuffer_pool_.pop();
            } else {
                driver_handle_ = drivers::create_renderbuffer(drv, size * current_scale_, format_driver);
                need_recreate = false;

                if (!driver_handle_) {
                    return GL_INVALID_OPERATION;
                }
            }
        }

        if (need_recreate) {
            context_.cmd_builder_.recreate_renderbuffer(driver_handle_, size * current_scale_, format_driver);
        }

        return 0;
    }

    void gles_renderbuffer_object::attach_to(gles_framebuffer_object *obj) {
        if (std::find(attached_fbs_.begin(), attached_fbs_.end(), obj) == attached_fbs_.end()) {
            attached_fbs_.push_back(obj);
        }
    }

    void gles_renderbuffer_object::detach_from(gles_framebuffer_object *obj) {
        auto ite = std::find(attached_fbs_.begin(), attached_fbs_.end(), obj);

        if (ite != attached_fbs_.end()) {
            attached_fbs_.erase(ite);
        }
    }

    void gles_renderbuffer_object::try_upscale() {
        float scale = context_.draw_surface_->backed_screen_->display_scale_factor;

        if (!driver_handle_) {
            current_scale_ = scale;
            return;
        }

        if (current_scale_ == scale) {
            return;
        }

        drivers::texture_format format_driver;
        switch (format_) {
        case GL_RGBA4_EMU:
            format_driver = drivers::texture_format::rgba4;
            break;

        case GL_RGB5_A1_EMU:
            format_driver = drivers::texture_format::rgb5_a1;
            break;

        case GL_RGB565_EMU:
            format_driver = drivers::texture_format::rgb565;
            break;

        case GL_DEPTH_COMPONENT16_EMU:
            format_driver = drivers::texture_format::depth16;
            break;

        case GL_STENCIL_INDEX8_EMU:
            format_driver = drivers::texture_format::stencil8;
            break;

        case GL_DEPTH24_STENCIL8_OES:
            format_driver = drivers::texture_format::depth24_stencil8;
            break;

        default:
            LOG_ERROR(HLE_DISPATCHER, "Unrecognised renderbuffer format to recreate for upscaling!");
            return;
        }

        current_scale_ = scale;
        context_.cmd_builder_.recreate_renderbuffer(driver_handle_, size_ * scale, format_driver);
    }

    gles_framebuffer_object::gles_framebuffer_object(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , attached_color_(nullptr)
        , attached_depth_(nullptr)
        , attached_stencil_(nullptr)
        , attached_color_face_index_(0)
        , attached_depth_face_index_(0)
        , attached_stencil_face_index_(0)
        , color_changed_(false)
        , depth_changed_(false)
        , stencil_changed_(false) {

    }

    void gles_framebuffer_object::force_detach(gles_driver_object *obj) {
        if (!obj) {
            return;
        }

        if (attached_color_ == obj) {
            attached_color_ = nullptr;
        }

        if (attached_depth_ == obj) {
            attached_depth_ = nullptr;
        }

        if (attached_stencil_ == obj) {
            attached_stencil_ = nullptr;
        }
    }

    gles_framebuffer_object::~gles_framebuffer_object() {
        if (attached_color_ && (attached_color_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
            reinterpret_cast<gles_renderbuffer_object*>(attached_color_)->detach_from(this);
        }

        if (attached_stencil_ && (attached_stencil_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
            reinterpret_cast<gles_renderbuffer_object*>(attached_stencil_)->detach_from(this);
        }

        if (attached_stencil_ && (attached_stencil_ != attached_depth_) && (attached_stencil_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
            reinterpret_cast<gles_renderbuffer_object*>(attached_stencil_)->detach_from(this);
        }

        if (driver_handle_) {
            egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
            es2_ctx.framebuffer_pool_.push(driver_handle_);
        }
    }

    std::uint32_t gles_framebuffer_object::set_attachment(gles_driver_object *object, const std::uint32_t attachment_type, const int face_index) {
        if (object && (object->object_type() != GLES_OBJECT_TEXTURE) && (object->object_type() != GLES_OBJECT_RENDERBUFFER)) {
            return GL_INVALID_OPERATION;
        }

        switch (attachment_type) {
        case GL_COLOR_ATTACHMENT0_EMU:
            if (attached_color_ != object) {
                if (attached_color_ && (attached_color_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_color_)->detach_from(this);
                }

                attached_color_ = object;

                if (attached_color_ && (attached_color_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_color_)->attach_to(this);
                }

                color_changed_ = true;
            }

            if (attached_color_face_index_ != face_index) {
                attached_color_face_index_ = face_index;
                color_changed_ = true;
            }

            break;

        case GL_DEPTH_ATTACHMENT_EMU:
            if (attached_depth_ != object) {
                if (attached_depth_ && (attached_depth_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_depth_)->detach_from(this);
                }

                attached_depth_ = object;
                
                if (attached_depth_ && (attached_depth_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_depth_)->attach_to(this);
                }

                depth_changed_ = true;
            }

            if (attached_depth_face_index_ != face_index) {
                attached_depth_face_index_ = face_index;
                depth_changed_ = true;
            }

            break;

        case GL_STENCIL_ATTACHMENT_EMU:
            if (attached_stencil_ != object) {
                if (attached_stencil_ && (attached_stencil_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_stencil_)->detach_from(this);
                }

                attached_stencil_ = object;

                if (attached_stencil_ && (attached_stencil_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_stencil_)->attach_to(this);
                }

                stencil_changed_ = true;
            }

            if (attached_stencil_face_index_ != face_index) {
                attached_stencil_face_index_ = face_index;
                stencil_changed_ = true;
            }

            break;

        case GL_DEPTH_STENCIL_OES_EMU:
            if (attached_depth_ != object) {
                if (attached_depth_ && (attached_depth_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_depth_)->detach_from(this);
                }

                attached_depth_ = object;
                attached_stencil_ = object;

                if (attached_depth_ && (attached_depth_->object_type() == GLES_OBJECT_RENDERBUFFER)) {
                    reinterpret_cast<gles_renderbuffer_object*>(attached_depth_)->attach_to(this);
                }

                depth_changed_ = true;
                stencil_changed_ = true;
            }

            if (attached_depth_face_index_ != face_index) {
                attached_depth_face_index_ = face_index;
                attached_stencil_face_index_ = face_index;

                depth_changed_ = true;
                stencil_changed_ = true;
            }

            break;
        }

        return GL_NO_ERROR;
    }

    std::uint32_t gles_framebuffer_object::completed() const {
        if (!attached_color_ && !attached_depth_ && !attached_stencil_) {
            return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EMU;
        }

        std::uint32_t internal_format = 0;

        eka2l1::vec2 available_sizes[3];
        std::uint32_t available_size_count = 0;

        if (attached_color_) {
            if (attached_color_->object_type() == GLES_OBJECT_TEXTURE) {
                const gles_driver_texture *tex_color = reinterpret_cast<const gles_driver_texture*>(attached_color_);

                internal_format = tex_color->internal_format();
                available_sizes[available_size_count++] = tex_color->size();
            } else {
                internal_format = reinterpret_cast<const gles_renderbuffer_object*>(attached_color_)->get_format();
                available_sizes[available_size_count++] = reinterpret_cast<const gles_renderbuffer_object*>(attached_color_)->get_size();
            }

            if ((internal_format != GL_RGBA4_EMU) && (internal_format != GL_RGB5_A1_EMU) && (internal_format != GL_RGB565_EMU)
                && (internal_format != GL_RGBA_EMU) && (internal_format != GL_RGB_EMU)) {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
            }
        }

        if (attached_depth_ || attached_stencil_) {
            if (attached_depth_ && (attached_depth_ == attached_stencil_)) {
                if (attached_depth_->object_type() == GLES_OBJECT_TEXTURE) {
                    if (reinterpret_cast<const gles_driver_texture*>(attached_depth_)->internal_format() != GL_DEPTH24_STENCIL8_OES) {
                        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                    }

                    available_sizes[available_size_count++] = reinterpret_cast<const gles_driver_texture*>(attached_depth_)->size();
                } else {
                    if (reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_format() != GL_DEPTH24_STENCIL8_OES) {
                        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                    }

                    available_sizes[available_size_count++] = reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_size();
                }
            } else {
                if (attached_depth_) {
                    if (attached_depth_->object_type() == GLES_OBJECT_TEXTURE) {
                        if (reinterpret_cast<const gles_driver_texture*>(attached_depth_)->internal_format() != GL_DEPTH_COMPONENT16_EMU) {
                            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                        }

                        available_sizes[available_size_count++] = reinterpret_cast<const gles_driver_texture*>(attached_depth_)->size();
                    } else {
                        if (reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_format() != GL_DEPTH_COMPONENT16_EMU) {
                            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                        }

                        available_sizes[available_size_count++] = reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_size();
                    }
                }

                if (attached_stencil_) {
                    if (attached_stencil_->object_type() == GLES_OBJECT_TEXTURE) {
                        if (reinterpret_cast<const gles_driver_texture*>(attached_stencil_)->internal_format() != GL_STENCIL_INDEX8_EMU) {
                            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                        }

                        available_sizes[available_size_count++] = reinterpret_cast<const gles_driver_texture*>(attached_stencil_)->size();
                    } else {
                        if (reinterpret_cast<const gles_renderbuffer_object*>(attached_stencil_)->get_format() != GL_STENCIL_INDEX8_EMU) {
                            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                        }

                        available_sizes[available_size_count++] = reinterpret_cast<const gles_renderbuffer_object*>(attached_stencil_)->get_size();
                    }
                }
            }
        }

        for (std::uint32_t i = 0; i < available_size_count - 1; i++) {
            if (available_sizes[i] != available_sizes[i + 1]) {
                return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EMU;
            }
        }

        return GL_FRAMEBUFFER_COMPLETE_EMU;
    }

    void gles_framebuffer_object::try_upscale_attachment(gles_driver_object *obj) {
        if (!obj) {
            return;
        }

        if (obj->object_type() == GLES_OBJECT_RENDERBUFFER) {
            gles_renderbuffer_object *renderbuffer = reinterpret_cast<gles_renderbuffer_object*>(obj);
            renderbuffer->try_upscale();
        } else {
            gles_driver_texture *texture = reinterpret_cast<gles_driver_texture*>(obj);
            texture->try_upscale();
        }
    }

    std::uint32_t gles_framebuffer_object::ready_for_draw(drivers::graphics_driver *drv) {
        std::uint32_t err = completed();
        
        if (err != GL_FRAMEBUFFER_COMPLETE_EMU) {
            return err;
        }

        egl_context_es2 &es2_context = static_cast<egl_context_es2&>(context_);
        bool need_manual_set = true;

        try_upscale_attachment(attached_color_);
        try_upscale_attachment(attached_depth_);

        if (attached_stencil_ != attached_depth_) {
            try_upscale_attachment(attached_stencil_);
        }

        if (!driver_handle_) {
            if (!es2_context.framebuffer_pool_.empty()) {
                driver_handle_ = es2_context.framebuffer_pool_.top();
            } else {
                drivers::handle color_buffer_local = attached_color_->handle_value();
                driver_handle_ = drivers::create_framebuffer(drv, &color_buffer_local, &attached_color_face_index_, 1,
                    attached_depth_ ? attached_depth_->handle_value() : 0, attached_depth_face_index_,
                    attached_stencil_ ? attached_stencil_->handle_value() : 0, attached_stencil_face_index_);

                if (!driver_handle_) {
                    LOG_ERROR(HLE_DISPATCHER, "Unable to instantiate framebuffer object!");
                    return GL_FRAMEBUFFER_UNSUPPORTED_EMU;
                }

                need_manual_set = false;
            }
        }

        if (need_manual_set) {
            if (color_changed_) {
                context_.cmd_builder_.set_framebuffer_color_buffer(driver_handle_, (attached_color_ ? 
                    attached_color_->handle_value() : 0), attached_color_face_index_, 0);

                color_changed_ = false;
            }

            if (depth_changed_ || stencil_changed_) {
                context_.cmd_builder_.set_framebuffer_depth_stencil_buffer(driver_handle_, attached_depth_ ? 
                    attached_depth_->handle_value() : 0, attached_depth_face_index_,
                    attached_stencil_ ? attached_stencil_->handle_value() : 0, attached_stencil_face_index_);

                depth_changed_ = false;
                stencil_changed_ = false;
            }
        }

        color_changed_ = false;
        depth_changed_ = false;
        stencil_changed_ = false;

        context_.cmd_builder_.bind_framebuffer(driver_handle_, drivers::framebuffer_bind_read_draw);
        return 0;
    }

    egl_context_es2::egl_context_es2()
        : binded_renderbuffer_(0)
        , binded_framebuffer_(0)
        , previous_using_program_(nullptr)
        , using_program_(nullptr)
        , framebuffer_need_reconfigure_(true)
        , attributes_enabled_(false)
        , previous_first_index_(0)
        , input_descs_(0) {
        std::fill(texture_units_.begin(), texture_units_.end(), 0);
    }

    void egl_context_es2::destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) {
        while (!compiled_shader_cleanup_.empty()) {
            builder.destroy(compiled_shader_cleanup_.front());
            compiled_shader_cleanup_.pop();
        }

        while (!linked_program_cleanup_.empty()) {
            builder.destroy(linked_program_cleanup_.front());
            linked_program_cleanup_.pop();
        }

        while (!renderbuffer_pool_.empty()) {
            builder.destroy(renderbuffer_pool_.top());
            renderbuffer_pool_.pop();
        }

        while (!framebuffer_pool_.empty()) {
            builder.destroy(framebuffer_pool_.top());
            framebuffer_pool_.pop();
        }

        if (input_descs_) {
            cmd_builder_.destroy(input_descs_);
        }

        egl_context_es_shared::destroy(driver, builder);
    }

    bool egl_context_es2::retrieve_vertex_buffer_slot(std::vector<drivers::handle> &vertex_buffers_alloc, drivers::graphics_driver *drv,
        kernel::process *crr_process, const gles_vertex_attrib &attrib, const std::int32_t first_index, const std::int32_t vcount,
        std::uint32_t &res, int &offset, bool &attrib_not_persistent) {
        const gles2_vertex_attrib &attrib_es2 = static_cast<const gles2_vertex_attrib&>(attrib);
        if (attrib_es2.use_constant_vcomp_ != 0) {
            if (attrib_es2.constant_vcomp_count_ == 0) {
                return true;
            }

            std::size_t offset_sizet = 0;

            if (!vertex_buffer_pusher_.is_initialized()) {
                vertex_buffer_pusher_.initialize(drv, common::MB(4));
            }

            // Upload constant data
            drivers::handle handle_retrieved = vertex_buffer_pusher_.push_buffer(reinterpret_cast<const std::uint8_t*>(attrib_es2.constant_data_),
                attrib_es2.constant_vcomp_count_ * 4, offset_sizet);

            offset = static_cast<int>(offset_sizet);
            auto ite = std::find(vertex_buffers_alloc.begin(), vertex_buffers_alloc.end(), handle_retrieved);
            if (ite != vertex_buffers_alloc.end()) {
                res = static_cast<std::uint32_t>(std::distance(vertex_buffers_alloc.begin(), ite));
            } else {
                res = static_cast<std::uint32_t>(vertex_buffers_alloc.size());
                vertex_buffers_alloc.push_back(handle_retrieved);
            }

            attrib_not_persistent = false;

            return true;
        }

        return egl_context_es_shared::retrieve_vertex_buffer_slot(vertex_buffers_alloc, drv, crr_process, attrib, first_index, vcount, res,
            offset, attrib_not_persistent);
    }

    static gles_framebuffer_object *get_binded_framebuffer_object_gles(egl_context_es2 *ctx, dispatch::egl_controller &controller) {
        gles_driver_object_instance *obj = ctx->objects_.get(ctx->binded_framebuffer_);
        if (!obj || ((*obj)->object_type() != GLES_OBJECT_FRAMEBUFFER)) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<gles_framebuffer_object*>(obj->get());
    }

    std::uint32_t egl_context_es2::bind_texture(const std::uint32_t target, const std::uint32_t tex) {
        auto *obj = objects_.get(tex);
        if (obj && (*obj).get()) {
            std::uint32_t bind_res = reinterpret_cast<gles_driver_texture*>((*obj).get())->try_bind(target);
            if (bind_res != 0) {
                return bind_res;
            }
        }

        texture_units_[active_texture_unit_] = tex;
        return 0;
    }
    
    gles_driver_texture *egl_context_es2::binded_texture() {
        if (texture_units_[active_texture_unit_] == 0) {
            return nullptr;
        }

        std::uint32_t handle = texture_units_[active_texture_unit_];

        auto *obj = objects_.get(handle);
        if (!obj || !obj->get() || (*obj)->object_type() != GLES_OBJECT_TEXTURE) {
            if (!obj->get()) {
                // The capacity is still enough. Someone has deleted the texture that should not be ! (yes, Pet Me by mBounce)
                LOG_WARN(HLE_DISPATCHER, "Texture name {} was previously deleted, generate a new one"
                    " (only because the slot is empty)!", handle);
                *obj = std::make_unique<gles_driver_texture>(*this);
            } else {
                return nullptr;
            }
        }

        return reinterpret_cast<gles_driver_texture*>(obj->get());
    }
    
    bool egl_context_es2::try_configure_framebuffer(drivers::graphics_driver *drv, egl_controller &controller) {
        if (framebuffer_need_reconfigure_) {
            if (!binded_framebuffer_) {
                draw_surface_->scale_and_bind(this, drv);
            } else {
                gles_framebuffer_object *fb_obj = get_binded_framebuffer_object_gles(this, controller);
                if (!fb_obj) {
                    return false;
                }

                std::uint32_t result = fb_obj->ready_for_draw(drv);
                if (result != 0) {
                    controller.push_error(this, result);
                    return false;
                }
            }

            framebuffer_need_reconfigure_ = false;
        }

        return true;
    }

    bool egl_context_es2::prepare_for_draw(drivers::graphics_driver *driver, egl_controller &controller, kernel::process *crr_process,
        const std::int32_t first_index, const std::uint32_t vcount) {
        if (!using_program_) {
            // What are you going to see?
            controller.push_error(this, GL_INVALID_OPERATION);
            return false;
        }

        if (!try_configure_framebuffer(driver, controller)) {
            return false;
        }

        flush_state_changes();

        if (!using_program_->prepare_for_draw()) {
            controller.push_error(this, GL_INVALID_OPERATION);
            return false;
        }

        if (!prepare_vertex_attributes(driver, crr_process, first_index, vcount)) {
            controller.push_error(this, GL_INVALID_OPERATION);
            return false;
        }

        for (std::size_t i = 0; i < texture_units_.size(); i++) {
            if (texture_units_[i] != 0) {
                auto *obj = objects_.get(texture_units_[i]);

                if (obj && obj->get())
                    cmd_builder_.bind_texture((*obj)->handle_value(), static_cast<int>(i));
            }
        }

        previous_using_program_ = using_program_;
        return true;
    }

    bool egl_context_es2::prepare_for_clear(drivers::graphics_driver *driver, egl_controller &controller) {
        if (!egl_context_es_shared::prepare_for_clear(driver, controller)) {
            return false;
        }

        return try_configure_framebuffer(driver, controller);
    }

    bool egl_context_es2::prepare_vertex_attributes(drivers::graphics_driver *drv, kernel::process *crr_process, const std::int32_t first_index, const std::int32_t vcount) {
        if (attrib_changed_ || (first_index != previous_first_index_)) {
            std::vector<drivers::input_descriptor> descs;
            std::vector<drivers::handle> vertex_buffers_alloc;

            drivers::input_descriptor desc_temp;
            drivers::data_format temp_format;

            bool attrib_not_persistent = false;

            for (std::uint32_t i = 0; i < GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT; i++) {
                if ((attributes_enabled_ & (1 << i)) == 0) {
                    attributes_[i].use_constant_vcomp_ = true;
                } else {
                    attributes_[i].use_constant_vcomp_ = false;
                }

                if (attributes_[i].use_constant_vcomp_ && attributes_[i].constant_vcomp_count_ == 0) {
                    //LOG_TRACE(HLE_DISPATCHER, "Attribute is disabled but does not have in-context vertex attribute value!");
                    continue;
                }

                if (attributes_[i].client_ask_instanced_ || attributes_[i].use_constant_vcomp_) {
                    desc_temp.set_per_instance(true);
                } else {
                    desc_temp.set_per_instance(false);
                }

                if (!retrieve_vertex_buffer_slot(vertex_buffers_alloc, drv, crr_process, attributes_[i], first_index, vcount, desc_temp.buffer_slot,
                    desc_temp.offset, attrib_not_persistent)) {
                    return false;
                }

                auto route_ite = attrib_bind_routes_.find(i);
                if (route_ite != attrib_bind_routes_.end()) {
                    desc_temp.location = route_ite->second;
                } else {
                    desc_temp.location = static_cast<int>(i);
                }

                if (attributes_[i].use_constant_vcomp_) {
                    desc_temp.set_normalized(false);
                    desc_temp.stride = attributes_[i].constant_vcomp_count_ * 4;
                    desc_temp.set_format(attributes_[i].constant_vcomp_count_, drivers::data_format::sfloat);
                } else {                    
                    desc_temp.set_normalized(attributes_[i].normalized_);
                    desc_temp.stride = attributes_[i].stride_;
                            
                    gl_enum_to_drivers_data_format(attributes_[i].data_type_, temp_format);
                    desc_temp.set_format(attributes_[i].size_, temp_format);
                }

                descs.push_back(desc_temp);
            }

            if (!input_descs_) {
                input_descs_ = drivers::create_input_descriptors(drv, descs.data(), static_cast<std::uint32_t>(descs.size()));
            } else {
                cmd_builder_.update_input_descriptors(input_descs_, descs.data(), static_cast<std::uint32_t>(descs.size()));
            }

            cmd_builder_.set_vertex_buffers(vertex_buffers_alloc.data(), 0, static_cast<std::uint32_t>(vertex_buffers_alloc.size()));

            if (!attrib_not_persistent) {
                attrib_changed_ = false;
            }

            previous_first_index_ = first_index;
        }
        
        cmd_builder_.bind_input_descriptors(input_descs_);
        return true;
    }

    template <typename T>
    bool get_data_impl_es2(egl_context_es2 *ctx, drivers::graphics_driver *drv, std::uint32_t pname, T *params, std::uint32_t scale_factor) {
        switch (pname) {
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_EMU:
            *params = static_cast<T>(GLES2_EMU_MAX_TEXTURE_UNIT_COUNT * 2);
            break;

        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_EMU:
        case GL_MAX_TEXTURE_IMAGE_UNITS_EMU:
            *params = static_cast<T>(GLES2_EMU_MAX_TEXTURE_UNIT_COUNT);
            break;

        case GL_MAX_VERTEX_ATTRIBS_EMU:
            *params = static_cast<T>(GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT);
            break;

        case GL_NUM_SHADER_BINARY_FORMATS_EMU:
            *params = static_cast<T>(0);
            break;

        case GL_FRAMEBUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->binded_framebuffer_);
            break;

        case GL_STENCIL_BITS_EMU:
            *params = static_cast<T>(8);
            break;

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es2::get_data(drivers::graphics_driver *drv, const std::uint32_t feature, void *data, gles_get_data_type data_type) {
        if (egl_context_es_shared::get_data(drv, feature, data, data_type)) {
            return true;
        }
        switch (data_type) {
        case GLES_GET_DATA_TYPE_BOOLEAN:
            return get_data_impl_es2<std::int32_t>(this, drv, feature, reinterpret_cast<std::int32_t*>(data), 255);

        case GLES_GET_DATA_TYPE_FIXED:
            return get_data_impl_es2<gl_fixed>(this, drv, feature, reinterpret_cast<gl_fixed*>(data), 65536);

        case GLES_GET_DATA_TYPE_FLOAT:
            return get_data_impl_es2<float>(this, drv, feature, reinterpret_cast<float*>(data), 1);

        case GLES_GET_DATA_TYPE_INTEGER:
            return get_data_impl_es2<std::uint32_t>(this, drv, feature, reinterpret_cast<std::uint32_t*>(data), 255);

        default:
            break;
        }

        return false;
    }

    void egl_context_es2::init_context_state() {
        if (using_program_ && using_program_->is_linked()) {
            cmd_builder_.use_program(using_program_->handle_value());
        }

        egl_context_es_shared::init_context_state();
    }

    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_create_shader_emu, const std::uint32_t shader_type) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return 0;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((shader_type != GL_VERTEX_SHADER_EMU) && (shader_type != GL_FRAGMENT_SHADER_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return 0;
        }

        gles_driver_object_instance empty_shader = std::make_unique<gles_shader_object>(*ctx,
            (shader_type == GL_VERTEX_SHADER_EMU) ? drivers::shader_module_type::vertex : drivers::shader_module_type::fragment);
        gles_shader_object *empty_shader_ptr = reinterpret_cast<gles_shader_object*>(empty_shader.get());

        const std::uint32_t cli_handle = static_cast<std::uint32_t>(ctx->objects_.add(empty_shader));
        empty_shader_ptr->assign_client_handle(cli_handle);

        return cli_handle;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_shader_binary_emu) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        controller.push_error(ctx, GL_INVALID_ENUM);
    }

    static gles_shader_object *get_shader_object_gles(egl_context_es2 *ctx, dispatch::egl_controller &controller, const std::uint32_t obj) {
        auto object_ptr = ctx->objects_.get(obj);
        if (!object_ptr) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return nullptr;
        }

        if (object_ptr->get()->object_type() != GLES_OBJECT_SHADER) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<gles_shader_object*>(object_ptr->get());
    }

    static constexpr std::size_t MAXIMUM_UNDELETED_SHADER_MODULE_HANDLE = 15;
    static void cleanup_pending_shader_driver_handle(egl_context_es2 *ctx, drivers::graphics_driver *driver) {
        if (ctx->compiled_shader_cleanup_.size() >= MAXIMUM_UNDELETED_SHADER_MODULE_HANDLE) {
            drivers::graphics_command_builder builder;
            while (!ctx->compiled_shader_cleanup_.empty()) {
                builder.destroy(ctx->compiled_shader_cleanup_.front());
                ctx->compiled_shader_cleanup_.pop();
            }

            drivers::command_list list = builder.retrieve_command_list();
            driver->submit_command_list(list);
        }

        return;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_delete_shader_emu, std::uint32_t shader) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        gles_shader_object *shader_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_obj) {
            return;
        }

        shader_obj->delete_object();
    }

    BRIDGE_FUNC_LIBRARY(void, gl_shader_source_emu, std::uint32_t shader, std::int32_t count, eka2l1::ptr<const char> *strings,
        const std::int32_t *length) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!strings) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (count <= 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        gles_shader_object *shader_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_obj) {
            return;
        }

        kernel_system *kern = sys->get_kernel_system();
        process_ptr crr_pr = kern->crr_process();

        std::string concentrated_source;
        std::string temp;

        for (std::int32_t i = 0; i < count; i++) {
            if (!length) {
                temp = strings[i].get(crr_pr);
            } else {
                const char *pointer_for_string = (*strings).get(crr_pr);
                temp = std::string(pointer_for_string, pointer_for_string + length[i]);
            }

            concentrated_source += temp;
        }

        shader_obj->set_source(concentrated_source);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_shader_iv_emu, std::uint32_t shader, std::uint32_t pname, std::int32_t *params) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_shader_object *shader_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_obj) {
            return;
        }

        if (!params) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        switch (pname) {
        case GL_SHADER_SOURCE_LENGTH_EMU: {
            *params = static_cast<std::int32_t>(shader_obj->get_source_length());
            break;

        case GL_SHADER_TYPE_EMU:
            *params = (shader_obj->get_shader_module_type() == drivers::shader_module_type::vertex) ? GL_VERTEX_SHADER_EMU
                : GL_FRAGMENT_SHADER_EMU;

            break;

        case GL_DELETE_STATUS_EMU:
            *params = static_cast<std::int32_t>(shader_obj->is_delete_pending());
            break;

        case GL_COMPILE_STATUS_EMU:
            *params = static_cast<std::int32_t>(shader_obj->is_last_compile_ok());
            break;

        case GL_INFO_LOG_LENGTH_EMU:
            *params = static_cast<std::int32_t>(shader_obj->get_compile_info_length());
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            break;
        }
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_compile_shader_emu, std::uint32_t shader) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_shader_object *shader_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_obj) {
            return;
        }

        drivers::graphics_driver *drv = sys->get_graphics_driver();

        cleanup_pending_shader_driver_handle(ctx, drv);
        shader_obj->compile(drv);
    }

    BRIDGE_FUNC_LIBRARY(bool, gl_is_shader_emu, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES_OBJECT_SHADER);
    }
    
    BRIDGE_FUNC_LIBRARY(bool, gl_is_program_emu, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES_OBJECT_PROGRAM);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_release_shader_compiler_emu) {
        // Intentionally empty
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_shader_source_emu, std::uint32_t shader, std::int32_t buf_size, std::int32_t *actual_length, char *source) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_shader_object *shader_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_obj) {
            return;
        }

        if (!source) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::int32_t length_to_copy = common::min<std::int32_t>(buf_size, static_cast<std::int32_t>(shader_obj->get_source_length()));
        const std::string &source_std = shader_obj->get_source();

        std::memcpy(source, source_std.data(), length_to_copy);
        if (actual_length) {
            *actual_length = length_to_copy;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_shader_info_log_emu, std::uint32_t shader, std::int32_t max_length, std::int32_t *actual_length, char *info_log) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_shader_object *shader_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_obj) {
            return;
        }

        if (!info_log) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::int32_t length_to_copy = common::min<std::int32_t>(max_length, static_cast<std::int32_t>(shader_obj->get_compile_info_length()));
        const std::string &info_std = shader_obj->get_compile_info();

        std::memcpy(info_log, info_std.data(), length_to_copy);
        if (actual_length) {
            *actual_length = length_to_copy;
        }
    }

    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_create_program_emu) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return 0;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_driver_object_instance empty_program = std::make_unique<gles_program_object>(*ctx);
        gles_program_object *empty_program_ptr = reinterpret_cast<gles_program_object*>(empty_program.get());

        const std::uint32_t cli_handle = static_cast<std::uint32_t>(ctx->objects_.add(empty_program));
        empty_program_ptr->assign_client_handle(cli_handle);

        return cli_handle;
    }
    
    static gles_program_object *get_program_object_gles(egl_context_es2 *ctx, dispatch::egl_controller &controller, const std::uint32_t obj) {
        auto object_ptr = ctx->objects_.get(obj);
        if (!object_ptr) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return nullptr;
        }

        if (object_ptr->get()->object_type() != GLES_OBJECT_PROGRAM) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<gles_program_object*>(object_ptr->get());
    }

    static constexpr std::size_t MAXIMUM_UNDELETED_PROGRAM_HANDLE = 10;
    static void cleanup_linked_program_driver_handle(egl_context_es2 *ctx, drivers::graphics_driver *driver) {
        if (ctx->linked_program_cleanup_.size() >= MAXIMUM_UNDELETED_SHADER_MODULE_HANDLE) {
            drivers::graphics_command_builder builder;
            while (!ctx->linked_program_cleanup_.empty()) {
                builder.destroy(ctx->linked_program_cleanup_.front());
                ctx->linked_program_cleanup_.pop();
            }

            drivers::command_list list = builder.retrieve_command_list();
            driver->submit_command_list(list);
        }

        return;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_attach_shader_emu, std::uint32_t program, std::uint32_t shader) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        gles_program_object *program_obj = get_program_object_gles(ctx, controller, program);

        if (!program_obj) {
            return;
        }

        gles_shader_object *shader_to_attach_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_to_attach_obj) {
            return;
        }

        if (!program_obj->attach(shader_to_attach_obj)) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_detach_shader_emu, std::uint32_t program, std::uint32_t shader) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        gles_program_object *program_obj = get_program_object_gles(ctx, controller, program);

        if (!program_obj) {
            return;
        }

        gles_shader_object *shader_to_detach_obj = get_shader_object_gles(ctx, controller, shader);
        if (!shader_to_detach_obj) {
            return;
        }

        if (!program_obj->detach(shader_to_detach_obj)) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_link_program_emu, std::uint32_t program) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        gles_program_object *program_obj = get_program_object_gles(ctx, controller, program);

        if (!program_obj) {
            return;
        }

        drivers::graphics_driver *drv = sys->get_graphics_driver();
        cleanup_linked_program_driver_handle(ctx, drv);
        program_obj->link(drv);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_validate_program_emu, std::uint32_t program) {
        // Left empty
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_program_iv_emu, std::uint32_t program, std::uint32_t pname, std::int32_t *params) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        gles_program_object *program_obj = get_program_object_gles(ctx, controller, program);

        if (!program_obj) {
            return;
        }

        switch (pname) {
        case GL_VALIDATE_STATUS_EMU:
            *params = 1;
            break;

        case GL_LINK_STATUS_EMU:
            *params = static_cast<std::int32_t>(program_obj->is_linked());
            break;

        case GL_DELETE_STATUS_EMU:
            *params = static_cast<std::int32_t>(program_obj->is_delete_pending());
            break;

        case GL_INFO_LOG_LENGTH_EMU:
            *params = program_obj->get_link_log_length();
            break;

        case GL_ATTACHED_SHADERS_EMU:
            *params = program_obj->attached_shaders_count();
            break;

        case GL_ACTIVE_UNIFORMS_EMU:
            *params = program_obj->active_uniforms_count();
            break;

        case GL_ACTIVE_ATTRIBUTES_EMU:
            *params = program_obj->active_attributes_count();
            break;
            
        case GL_ACTIVE_UNIFORM_MAX_LENGTH_EMU:
            *params = program_obj->active_uniform_max_name_length();
            break;

        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH_EMU:
            *params = program_obj->active_attribute_max_name_length();
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            break;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_program_info_log_emu, std::uint32_t program, std::int32_t max_length, std::int32_t *actual_length, char *info_log) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        auto object_ptr = ctx->objects_.get(program);
        if (!object_ptr) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (object_ptr->get()->object_type() == GLES_OBJECT_SHADER) {
            gl_get_shader_info_log_emu(sys, program, max_length, actual_length, info_log);
            return;
        } else if (object_ptr->get()->object_type() != GLES_OBJECT_PROGRAM) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        gles_program_object *program_obj = reinterpret_cast<gles_program_object*>(object_ptr->get());

        if (!info_log) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::int32_t length_to_copy = common::min<std::int32_t>(max_length, static_cast<std::int32_t>(program_obj->get_link_log_length()));
        const std::string &info_std = program_obj->get_link_log();

        std::memcpy(info_log, info_std.data(), length_to_copy);
        if (actual_length) {
            *actual_length = length_to_copy;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_gen_renderbuffers_emu, std::int32_t count, std::uint32_t *rbs) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
    
        if (count < 0 || !rbs) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (count == 0) {
            return;
        }

        gles_driver_object_instance stub_obj;

        for (std::int32_t i = 0; i < count; i++) {
            stub_obj = std::make_unique<gles_renderbuffer_object>(*ctx);
            rbs[i] = static_cast<std::uint32_t>(ctx->objects_.add(stub_obj));
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_bind_renderbuffer_emu, std::uint32_t target, std::uint32_t rb) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_RENDERBUFFER_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->binded_renderbuffer_ = rb;
    }
    
    BRIDGE_FUNC_LIBRARY(bool, gl_is_renderbuffer_emu, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES_OBJECT_RENDERBUFFER);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_renderbuffer_storage_emu, std::uint32_t target, std::uint32_t internal_format, std::int32_t width, std::int32_t height) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_RENDERBUFFER_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
        
        gles_driver_object_instance *obj = ctx->objects_.get(ctx->binded_renderbuffer_);
        if (!obj || ((*obj)->object_type() != GLES_OBJECT_RENDERBUFFER)) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        gles_renderbuffer_object *rb_obj = reinterpret_cast<gles_renderbuffer_object*>(obj->get());
        const std::uint32_t err = rb_obj->make_storage(sys->get_graphics_driver(), eka2l1::vec2(width, height), internal_format);

        if (err != 0) {
            controller.push_error(ctx, err);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_gen_framebuffers_emu, std::int32_t count, std::uint32_t *names) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
    
        if (count < 0 || !names) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (count == 0) {
            return;
        }

        gles_driver_object_instance stub_obj;

        for (std::int32_t i = 0; i < count; i++) {
            stub_obj = std::make_unique<gles_framebuffer_object>(*ctx);
            names[i] = static_cast<std::uint32_t>(ctx->objects_.add(stub_obj));
        }
    }

    BRIDGE_FUNC_LIBRARY(bool, gl_is_framebuffer_emu, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES_OBJECT_FRAMEBUFFER);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_bind_framebuffer_emu, std::uint32_t target, std::uint32_t fb) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_FRAMEBUFFER_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (ctx->binded_framebuffer_ != fb) {
            ctx->binded_framebuffer_ = fb;
            ctx->framebuffer_need_reconfigure_ = true;
        }
    }

    static void submit_framebuffer_attachment_gles(system *sys, std::uint32_t target, std::uint32_t attachment_type,
        std::uint32_t attachment_target, std::uint32_t attachment, std::uint32_t target_attachment_target) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_framebuffer_object *fb_obj = get_binded_framebuffer_object_gles(ctx, controller);
        if (!fb_obj) {
            return;
        }

        int face_index = 0;

        if (target_attachment_target == GL_TEXTURE_2D_EMU) {
            if ((attachment_target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU) && (attachment_target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EMU)) {
                face_index = static_cast<int>(attachment_target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU);
            } else if (attachment_target != GL_TEXTURE_2D_EMU) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }
        } else if (attachment_target != target_attachment_target) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(attachment);
        if (!obj) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        std::uint32_t result = fb_obj->set_attachment(obj->get(), attachment_type, face_index);
        if (result != 0) {
            controller.push_error(ctx, result);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_framebuffer_texture2d_emu, std::uint32_t target, std::uint32_t attachment_type,
        std::uint32_t attachment_target, std::uint32_t attachment) {
        submit_framebuffer_attachment_gles(sys, target, attachment_type, attachment_target, attachment, GL_TEXTURE_2D_EMU);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_framebuffer_renderbuffer_emu, std::uint32_t target, std::uint32_t attachment_type,
        std::uint32_t attachment_target, std::uint32_t attachment) {
        submit_framebuffer_attachment_gles(sys, target, attachment_type, attachment_target, attachment, GL_RENDERBUFFER_EMU);
    }
    
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_check_framebuffer_status_emu, std::uint32_t target) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return GL_INVALID_OPERATION;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_FRAMEBUFFER_EMU) {
            return GL_INVALID_ENUM;
        }

        if (ctx->binded_framebuffer_ == 0) {
            return GL_FRAMEBUFFER_COMPLETE_EMU;
        }

        gles_framebuffer_object *current_binded = get_binded_framebuffer_object_gles(ctx, controller);
        if (!current_binded) {
            return GL_INVALID_OPERATION;
        }

        return current_binded->completed();
    }

    BRIDGE_FUNC_LIBRARY(void, gl_delete_framebuffers_emu, std::int32_t count, std::uint32_t *names) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        delete_gles_objects_generic(sys, GLES_OBJECT_FRAMEBUFFER, count, names);

        if (count <= 0) {
            return;
        }

        for (std::int32_t i = 0; i < count; i++) {
            if (names[i] == ctx->binded_framebuffer_) {
                ctx->binded_framebuffer_ = 0;
                ctx->framebuffer_need_reconfigure_ = true;
            }
        }
    }

    static std::uint32_t driver_shader_var_type_to_gles_enum(const drivers::shader_var_type var_type) {
        switch (var_type) {
        case drivers::shader_var_type::real:
            return GL_FLOAT_EMU;

        case drivers::shader_var_type::boolean:
            return GL_BOOL_EMU;

        case drivers::shader_var_type::integer:
            return GL_INT_EMU;
        
        case drivers::shader_var_type::bvec2:
            return GL_BOOL_VEC2_EMU;

        case drivers::shader_var_type::bvec3:
            return GL_BOOL_VEC3_EMU;

        case drivers::shader_var_type::bvec4:
            return GL_BOOL_VEC4_EMU;

        case drivers::shader_var_type::ivec2:
            return GL_INT_VEC2_EMU;

        case drivers::shader_var_type::ivec3:
            return GL_INT_VEC3_EMU;

        case drivers::shader_var_type::vec2:
            return GL_FLOAT_VEC2_EMU;

        case drivers::shader_var_type::vec3:
            return GL_FLOAT_VEC3_EMU;

        case drivers::shader_var_type::vec4:
            return GL_FLOAT_VEC4_EMU;

        case drivers::shader_var_type::mat2:
            return GL_FLOAT_MAT2_EMU;

        case drivers::shader_var_type::mat3:
            return GL_FLOAT_MAT3_EMU;

        case drivers::shader_var_type::mat4:
            return GL_FLOAT_MAT4_EMU;

        case drivers::shader_var_type::sampler2d:
            return GL_SAMPLER_2D_EMU;

        case drivers::shader_var_type::sampler_cube:
            return GL_SAMPLER_CUBE_EMU;

        default:
            break;
        }

        return GL_INT_EMU;
    }

    static void get_active_thing_info_gles(system *sys, const bool is_uniform, std::uint32_t program, std::uint32_t index, std::int32_t buf_size,
        std::int32_t *buf_written, std::int32_t *arr_len, std::uint32_t *var_type, char *name_buf) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (buf_size < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        gles_program_object *obj = get_program_object_gles(ctx, controller, program);
        if (!obj) {
            return;
        }

        if ((index >= static_cast<std::uint32_t>(obj->active_uniforms_count())) || (index < 0)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        const drivers::shader_program_metadata *metadata = obj->get_readonly_metadata();
        if (!metadata) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::string name;
        drivers::shader_var_type driver_var_type;

        std::int32_t binding_temp = 0;
        std::int32_t arr_size_driver = 0;

        if (is_uniform) {
            if (!metadata->get_uniform_info(static_cast<int>(index), name, binding_temp, driver_var_type, arr_size_driver)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }
        } else {
            if (!metadata->get_attribute_info(static_cast<int>(index), name, binding_temp, driver_var_type, arr_size_driver)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }
        }

        if (var_type) {
            *var_type = driver_shader_var_type_to_gles_enum(driver_var_type);
        }

        if (arr_len) {
            *arr_len = arr_size_driver;
        }

        if ((buf_size > 0) && name_buf) {
            std::int32_t size_to_copy = common::min<std::int32_t>(buf_size, static_cast<std::int32_t>(name.length()));
            std::memcpy(name_buf, name.data(), size_to_copy);
            name_buf[size_to_copy] = '\0';

            if (buf_written) {
                *buf_written = size_to_copy;
            }
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_active_uniform_emu, std::uint32_t program, std::uint32_t index, std::int32_t buf_size, std::int32_t *buf_written, 
        std::int32_t *arr_len, std::uint32_t *var_type, char *name_buf) {
        get_active_thing_info_gles(sys, true, program, index, buf_size, buf_written, arr_len, var_type, name_buf);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_active_attrib_emu, std::uint32_t program, std::uint32_t index, std::int32_t buf_size, std::int32_t *buf_written,
        std::int32_t *arr_len, std::uint32_t *var_type, char *name_buf) {
        get_active_thing_info_gles(sys, false, program, index, buf_size, buf_written, arr_len, var_type, name_buf);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_attached_shaders_emu, std::uint32_t program, std::int32_t max_buffer_count, std::int32_t *max_buffer_written,
        std::uint32_t *shaders) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((max_buffer_count < 0) || !shaders) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (max_buffer_count == 0) {
            if (max_buffer_written) {
                *max_buffer_written = 0;
            }

            return;
        }

        gles_program_object *obj = get_program_object_gles(ctx, controller, program);
        if (!obj) {
            return;
        }

        std::int32_t current_index = 0;
        if (obj->vertex_shader_handle() != 0) {
            shaders[current_index++] = obj->client_handle();
        }

        if (current_index < max_buffer_count) {
            if (obj->fragment_shader_handle() != 0) {
                shaders[current_index++] = obj->client_handle();
            }
        }

        if (max_buffer_written) {
            *max_buffer_written = current_index;
        }
    }

    static std::int32_t get_variable_location_gles2(system *sys, std::uint32_t program, const char *name, const bool is_uniform) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return -1;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!name) {
            return -1;
        }

        gles_program_object *obj = get_program_object_gles(ctx, controller, program);
        if (!obj) {
            return -1;
        }

        if (!obj->is_linked()) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return -1;
        }

        return is_uniform ? obj->get_readonly_metadata()->get_uniform_binding(name) : obj->get_readonly_metadata()->get_attribute_binding(name);
    }

    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_get_uniform_location_emu, std::uint32_t program, const char *name) {
        return get_variable_location_gles2(sys, program, name, true);
    }

    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_get_attrib_location_emu, std::uint32_t program, const char *name) {
        return get_variable_location_gles2(sys, program, name, false);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_use_program_emu, std::uint32_t program) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (program == 0) {
            ctx->using_program_ = nullptr;
        } else {
            gles_program_object *program_obj = get_program_object_gles(ctx, controller, program);

            if (!program_obj) {
                return;
            }

            ctx->using_program_ = program_obj;
        }
    }

    static void set_uniform_value_gles2(system *sys, int binding, void *data, std::int32_t total_size,
        std::int32_t count, drivers::shader_var_type var_type, std::uint32_t extra_flags) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!ctx->using_program_) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        std::uint32_t res = ctx->using_program_->set_uniform_data(binding, reinterpret_cast<const std::uint8_t*>(data), total_size,
            count, var_type, extra_flags);

        if (res != GL_NO_ERROR) {
            controller.push_error(ctx, res);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1f_emu, int location, float v0) {
        set_uniform_value_gles2(sys, location, &v0, 4, 1, drivers::shader_var_type::real, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2f_emu, int location, float v0, float v1) {
        float collection[2] = { v0, v1 };
        set_uniform_value_gles2(sys, location, collection, 8, 1, drivers::shader_var_type::vec2, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3f_emu, int location, float v0, float v1, float v2) {
        float collection[3] = { v0, v1, v2 };
        set_uniform_value_gles2(sys, location, collection, 12, 1, drivers::shader_var_type::vec3, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4f_emu, int location, float v0, float v1, float v2, float v3) {
        float collection[4] = { v0, v1, v2, v3 };
        set_uniform_value_gles2(sys, location, collection, 16, 1, drivers::shader_var_type::vec4, 0);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1fv_emu, int location, std::int32_t count, float *value) {
        set_uniform_value_gles2(sys, location, value, 4 * count, count, drivers::shader_var_type::real, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2fv_emu, int location, std::int32_t count, float *value) {
        set_uniform_value_gles2(sys, location, value, 8 * count, count, drivers::shader_var_type::vec2, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3fv_emu, int location, std::int32_t count, float *value) {
        set_uniform_value_gles2(sys, location, value, 12 * count, count, drivers::shader_var_type::vec3, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4fv_emu, int location, std::int32_t count, float *value) {
        set_uniform_value_gles2(sys, location, value, 16 * count, count, drivers::shader_var_type::vec4, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1i_emu, int location, std::int32_t i0) {
        set_uniform_value_gles2(sys, location, &i0, 4, 1, drivers::shader_var_type::integer, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2i_emu, int location, std::int32_t i0, std::int32_t i1) {
        std::int32_t collection[2] = { i0, i1 };
        set_uniform_value_gles2(sys, location, collection, 8, 1, drivers::shader_var_type::ivec2, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3i_emu, int location, std::int32_t i0, std::int32_t i1, std::int32_t i2) {
        std::int32_t collection[3] = { i0, i1, i2 };
        set_uniform_value_gles2(sys, location, collection, 12, 1, drivers::shader_var_type::ivec3, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4i_emu, int location, std::int32_t i0, std::int32_t i1, std::int32_t i2, std::int32_t i3) {
        std::int32_t collection[4] = { i0, i1, i2, i3 };
        set_uniform_value_gles2(sys, location, collection, 16, 1, drivers::shader_var_type::ivec4, 0);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1iv_emu, int location, std::int32_t count, std::int32_t *value) {
        set_uniform_value_gles2(sys, location, value, 4 * count, count, drivers::shader_var_type::integer, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2iv_emu, int location, std::int32_t count, std::int32_t *value) {
        set_uniform_value_gles2(sys, location, value, 8 * count, count, drivers::shader_var_type::ivec2, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3iv_emu, int location, std::int32_t count, std::int32_t *value) {
        set_uniform_value_gles2(sys, location, value, 12 * count, count, drivers::shader_var_type::ivec3, 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4iv_emu, int location, std::int32_t count, std::int32_t *value) {
        set_uniform_value_gles2(sys, location, value, 16 * count, count, drivers::shader_var_type::ivec4, 0);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_matrix_2fv_emu, int location, std::int32_t count, bool transpose, float *value) {
        set_uniform_value_gles2(sys, location, value, 16 * count, count, drivers::shader_var_type::mat2, transpose ? gles_uniform_variable_data_info::EXTRA_FLAG_MATRIX_NEED_TRANSPOSE : 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_matrix_3fv_emu, int location, std::int32_t count, bool transpose, float *value) {
        set_uniform_value_gles2(sys, location, value, 36 * count, count, drivers::shader_var_type::mat3, transpose ? gles_uniform_variable_data_info::EXTRA_FLAG_MATRIX_NEED_TRANSPOSE : 0);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_uniform_matrix_4fv_emu, int location, std::int32_t count, bool transpose, float *value) {
        set_uniform_value_gles2(sys, location, value, 64 * count, count, drivers::shader_var_type::mat4, transpose ? gles_uniform_variable_data_info::EXTRA_FLAG_MATRIX_NEED_TRANSPOSE : 0);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_enable_vertex_attrib_array_emu, int index) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if ((index < 0) || (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if ((ctx->attributes_enabled_ & (1 << index)) == 0) {
            ctx->attributes_enabled_ |= (1 << index);
            ctx->attrib_changed_ = true;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_disable_vertex_attrib_array_emu, int index) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if ((index < 0) || (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (ctx->attributes_enabled_ & (1 << index)) {
            ctx->attributes_enabled_ &= ~(1 << index);
            ctx->attrib_changed_ = true;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_1f_emu, std::uint32_t index, float v0) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->attributes_[index].constant_data_[0] = v0;
        ctx->attributes_[index].constant_vcomp_count_ = 1;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_2f_emu, std::uint32_t index, float v0, float v1) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->attributes_[index].constant_data_[0] = v0;
        ctx->attributes_[index].constant_data_[1] = v1;
        ctx->attributes_[index].constant_vcomp_count_ = 2;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_3f_emu, std::uint32_t index, float v0, float v1, float v2) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->attributes_[index].constant_data_[0] = v0;
        ctx->attributes_[index].constant_data_[1] = v1;
        ctx->attributes_[index].constant_data_[2] = v2;
        ctx->attributes_[index].constant_vcomp_count_ = 3;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_4f_emu, std::uint32_t index, float v0, float v1, float v2, float v3) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->attributes_[index].constant_data_[0] = v0;
        ctx->attributes_[index].constant_data_[1] = v1;
        ctx->attributes_[index].constant_data_[2] = v2;
        ctx->attributes_[index].constant_data_[3] = v3;
        ctx->attributes_[index].constant_vcomp_count_ = 4;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_1fv_emu, std::uint32_t index, float *v) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if ((index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) || !v) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->attributes_[index].constant_data_[0] = *v;
        ctx->attributes_[index].constant_vcomp_count_ = 1;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_2fv_emu, std::uint32_t index, float *v) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if ((index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) || !v) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::memcpy(ctx->attributes_[index].constant_data_, v, 8);
        ctx->attributes_[index].constant_vcomp_count_ = 2;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_3fv_emu, std::uint32_t index, float *v) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        if ((index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) || !v) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::memcpy(ctx->attributes_[index].constant_data_, v, 12);
        ctx->attributes_[index].constant_vcomp_count_ = 3;
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_4fv_emu, std::uint32_t index, float *v) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) || !v) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::memcpy(ctx->attributes_[index].constant_data_, v, 16);
        ctx->attributes_[index].constant_vcomp_count_ = 4;
        ctx->attrib_changed_ = true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_pointer_emu, std::uint32_t index, std::int32_t size, std::uint32_t type, bool normalized,
        std::int32_t stride, address offset) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->attributes_[index], size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attributes_[index].constant_vcomp_count_ = 0;
        ctx->attributes_[index].normalized_ = normalized;
        ctx->attrib_changed_ = true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_bind_attrib_location_emu, std::uint32_t program, std::uint32_t index, const char *name) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        gles_program_object *program_obj = get_program_object_gles(ctx, controller, program);

        if (!program_obj) {
            return;
        }

        if ((index >= GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT) || !name) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        const std::int32_t real_binding = program_obj->get_readonly_metadata()->get_attribute_binding(name);
        if (real_binding >= 0) {
            auto real_binding_stored_ite = ctx->attrib_bind_routes_.find(index);
            if ((real_binding_stored_ite != ctx->attrib_bind_routes_.end()) && (real_binding_stored_ite->second != real_binding)) {
                ctx->attrib_changed_ = true;
            }
            ctx->attrib_bind_routes_[index] = real_binding;
        }
    }
}