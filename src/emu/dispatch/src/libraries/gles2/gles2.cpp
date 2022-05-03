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

#include <dispatch/libraries/gles2/gles2.h>
#include <dispatch/libraries/gles2/def.h>

#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <system/epoc.h>
#include <kernel/kernel.h>

namespace eka2l1::dispatch {    
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

        cleanup_current_driver_module();
        driver_handle_ = drivers::create_shader_module(drv, source_.data(), source_.length(), module_type_, &compile_info_);

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

    gles_shader_object::~gles_shader_object() {
        cleanup_current_driver_module();
    }
    
    gles_renderbuffer_object::gles_renderbuffer_object(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , format_(0) {
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

        default:
            return GL_INVALID_ENUM;
        }

        format_ = format;
        size_ = size;

        egl_context_es2 &es2_ctx = static_cast<egl_context_es2&>(context_);
        bool need_recreate = true;

        if (!driver_handle_) {
            if (!es2_ctx.renderbuffer_pool_.empty()) {
                driver_handle_ = es2_ctx.renderbuffer_pool_.top();
                es2_ctx.renderbuffer_pool_.pop();
            } else {
                driver_handle_ = drivers::create_renderbuffer(drv, size, format_driver);
                need_recreate = false;

                if (!driver_handle_) {
                    return GL_INVALID_OPERATION;
                }
            }
        }

        if (need_recreate) {
            context_.cmd_builder_.recreate_renderbuffer(driver_handle_, size, format_driver);
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

    gles_framebuffer_object::gles_framebuffer_object(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , attached_color_(nullptr)
        , attached_depth_(nullptr)
        , attached_stencil_(nullptr)
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

    std::uint32_t gles_framebuffer_object::set_attachment(gles_driver_object *object, const std::uint32_t attachment_type) {
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

                depth_changed_ = true;
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

            break;
        }

        return GL_NO_ERROR;
    }

    std::uint32_t gles_framebuffer_object::completed() const {
        if (!attached_color_ || !attached_depth_ || !attached_stencil_) {
            return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EMU;
        }

        std::uint32_t internal_format = 0;

        eka2l1::vec2 color_size;
        eka2l1::vec2 depth_size;
        eka2l1::vec2 stencil_size;

        if (attached_color_->object_type() == GLES_OBJECT_TEXTURE) {
            internal_format = reinterpret_cast<const gles_driver_texture*>(attached_color_)->internal_format();
            color_size = reinterpret_cast<const gles_driver_texture*>(attached_color_)->size();
        } else {
            internal_format = reinterpret_cast<const gles_renderbuffer_object*>(attached_color_)->get_format();
            color_size = reinterpret_cast<const gles_renderbuffer_object*>(attached_color_)->get_size();
        }

        if ((internal_format != GL_RGBA4_EMU) && (internal_format != GL_RGB5_A1_EMU) && (internal_format != GL_RGB565_EMU)) {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
        }

        if (attached_depth_ == attached_stencil_) {
            if (attached_depth_->object_type() == GLES_OBJECT_TEXTURE) {
                if (reinterpret_cast<const gles_driver_texture*>(attached_depth_)->internal_format() != GL_DEPTH24_STENCIL8_OES) {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                }

                depth_size = reinterpret_cast<const gles_driver_texture*>(attached_depth_)->size();
            } else {
                if (reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_format() != GL_DEPTH24_STENCIL8_OES) {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                }

                depth_size = reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_size();
            }

            stencil_size = depth_size;
        } else {
            if (attached_depth_->object_type() == GLES_OBJECT_TEXTURE) {
                if (reinterpret_cast<const gles_driver_texture*>(attached_depth_)->internal_format() != GL_DEPTH_COMPONENT16_EMU) {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                }

                depth_size = reinterpret_cast<const gles_driver_texture*>(attached_depth_)->size();
            } else {
                if (reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_format() != GL_DEPTH_COMPONENT16_EMU) {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                }

                depth_size = reinterpret_cast<const gles_renderbuffer_object*>(attached_depth_)->get_size();
            }

            if (attached_stencil_->object_type() == GLES_OBJECT_TEXTURE) {
                if (reinterpret_cast<const gles_driver_texture*>(attached_stencil_)->internal_format() != GL_STENCIL_INDEX8_EMU) {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                }

                stencil_size = reinterpret_cast<const gles_driver_texture*>(attached_stencil_)->size();
            } else {
                if (reinterpret_cast<const gles_renderbuffer_object*>(attached_stencil_)->get_format() != GL_STENCIL_INDEX8_EMU) {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EMU;
                }

                stencil_size = reinterpret_cast<const gles_renderbuffer_object*>(attached_stencil_)->get_size();
            }
        }

        if ((color_size != depth_size) || (depth_size != stencil_size) || (color_size != stencil_size)) {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EMU;
        }

        return GL_FRAMEBUFFER_COMPLETE_EMU;
    }

    std::uint32_t gles_framebuffer_object::ready_for_draw(drivers::graphics_driver *drv) {
        std::uint32_t err = completed();
        
        if (err != 0) {
            return err;
        }

        egl_context_es2 &es2_context = static_cast<egl_context_es2&>(context_);
        bool need_manual_set = true;

        if (!driver_handle_) {
            if (!es2_context.framebuffer_pool_.empty()) {
                driver_handle_ = es2_context.framebuffer_pool_.top();
            } else {
                drivers::handle color_buffer_local = attached_color_->handle_value();
                driver_handle_ = drivers::create_framebuffer(drv, &color_buffer_local, 1, attached_depth_ ? attached_depth_->handle_value() : 0,
                    attached_stencil_ ? attached_stencil_->handle_value() : 0);

                if (!driver_handle_) {
                    LOG_ERROR(HLE_DISPATCHER, "Unable to instantiate framebuffer object!");
                    return GL_FRAMEBUFFER_UNSUPPORTED_EMU;
                }

                need_manual_set = false;
            }
        }

        if (need_manual_set) {
            if (color_changed_) {
                context_.cmd_builder_.set_framebuffer_color_buffer(driver_handle_, attached_color_ ? 
                    attached_color_->handle_value() : 0, 0);

                color_changed_ = false;
            }

            if (depth_changed_ || stencil_changed_) {
                context_.cmd_builder_.set_framebuffer_depth_stencil_buffer(driver_handle_, attached_depth_ ? 
                    attached_depth_->handle_value() : 0, attached_stencil_ ? attached_stencil_->handle_value() : 0);

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
        , framebuffer_need_reconfigure_(false) {  
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

        egl_context_es_shared::destroy(driver, builder);
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

        const std::uint32_t cli_handle = static_cast<std::uint32_t>(ctx->objects_.add(empty_shader));
        empty_shader->assign_client_handle(cli_handle);

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
                // Use std::string to calculate the null-terminated string length
                temp = (*strings).get(crr_pr);
                (*strings) += static_cast<address>(temp.length() + 1);
            } else {
                temp = strings[i].get(crr_pr);
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

        const std::uint32_t cli_handle = static_cast<std::uint32_t>(ctx->objects_.add(empty_program));
        empty_program->assign_client_handle(cli_handle);

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
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_program_info_log_emu, std::uint32_t shader, std::int32_t max_length, std::int32_t *actual_length, char *info_log) {
        egl_context_es2 *ctx = get_es2_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_program_object *program_obj = get_program_object_gles(ctx, controller, shader);
        if (!program_obj) {
            return;
        }

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

    static gles_framebuffer_object *get_binded_framebuffer_object_gles(egl_context_es2 *ctx, dispatch::egl_controller &controller) {
        gles_driver_object_instance *obj = ctx->objects_.get(ctx->binded_renderbuffer_);
        if (!obj || ((*obj)->object_type() != GLES_OBJECT_FRAMEBUFFER)) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<gles_framebuffer_object*>(obj->get());
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

        if (attachment_target != target_attachment_target) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(attachment);
        if (!obj) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        std::uint32_t result = fb_obj->set_attachment(obj->get(), attachment_type);
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
}