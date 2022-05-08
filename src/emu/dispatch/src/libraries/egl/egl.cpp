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

#include <dispatch/libraries/egl/egl.h>
#include <dispatch/libraries/gles1/def.h>
#include <dispatch/libraries/gles2/def.h>
#include <dispatch/dispatcher.h>
#include <kernel/kernel.h>

#include <system/epoc.h>
#include <services/window/window.h>
#include <services/window/classes/winuser.h>
#include <services/fbs/bitmap.h>
#include <services/fbs/fbs.h>

#include <drivers/itc.h>
#include <drivers/graphics/graphics.h>

#include <utils/guest/fbs.h>

namespace eka2l1::dispatch {
    // First bit is surface type, and second bit is buffer size bits
    static constexpr std::uint32_t EGL_EMU_CONFIG_LIST_VALS[] = {
        // GLES2
        0b1010, 0b1100, 0b1110,
        0b1011, 0b1101, 0b1111,
        // GLES1
        0b010, 0b100, 0b110,
        0b011, 0b101, 0b111,
    };

    static constexpr std::uint32_t EGL_EMU_CONFIG_LIST_VALS_LENGTH = sizeof(EGL_EMU_CONFIG_LIST_VALS) / sizeof(std::uint32_t);

    static void egl_push_error(system *sys, const std::int32_t error) {
        dispatcher *dp = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *crr_thread = kern->crr_thread();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        controller.push_egl_error(crr_thread->unique_id(), error);
    }

    BRIDGE_FUNC_LIBRARY(std::int32_t, egl_get_error) {
        dispatcher *dp = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();
        kernel::thread *crr_thread = kern->crr_thread();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        return controller.pop_egl_error(crr_thread->unique_id());
    }

    BRIDGE_FUNC_LIBRARY(egl_display, egl_get_display_emu, std::int32_t display_index) {
        dispatcher *dp = sys->get_dispatcher();

        if (display_index == 0) {
            return static_cast<egl_display>(dp->winserv_->get_current_focus_screen()->scr_config.screen_number + 1);
        }

        epoc::screen *scr = dp->winserv_->get_screen(display_index);
        if (scr) {
            return static_cast<egl_display>(display_index + 1);
        }

        return 0;
    }

    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_initialize_emu, egl_display display, std::int32_t *major, std::int32_t *minor) {
        if (major) {
            *major = EGL_MAJOR_VERSION_EMU;
        }

        if (minor) {
            *minor = EGL_MINOR_VERSION_EMU;
        }

        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_get_configs_emu, egl_display display, egl_config *configs, std::int32_t config_array_size, std::int32_t *config_total_size) {        
        if (!config_total_size) {
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
            return EGL_FALSE;
        }

        if (configs) {
            std::size_t length_to_fill = common::min(static_cast<std::uint32_t>(config_array_size), EGL_EMU_CONFIG_LIST_VALS_LENGTH);

            for (std::size_t i = 0; i < length_to_fill; i++) {
                configs[i].config_ = EGL_EMU_CONFIG_LIST_VALS[i];
            }
        }

        *config_total_size = EGL_EMU_CONFIG_LIST_VALS_LENGTH;
        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_choose_config_emu, egl_display display, std::int32_t *attrib_lists, egl_config *configs, std::int32_t config_array_size, std::int32_t *num_config_choosen) {
        if (!attrib_lists) {
            egl_push_error(sys, EGL_BAD_ATTRIBUTE_EMU);
            return EGL_FALSE;
        }

        if (!num_config_choosen) {
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
            return EGL_FALSE;
        }

        std::int32_t surface_type = -1;
        std::uint32_t total_color_bits_calculated = -1;
        std::uint32_t total_color_bits_provided = -1;
        std::uint32_t gles_context_type = EGL_OPENGL_ES1_BIT;

        while (attrib_lists != nullptr) {
            std::uint32_t pname = *attrib_lists++;
            if (pname == 0) {
                break;
            }
            std::uint32_t param = *attrib_lists++;

            switch (pname) {
            case EGL_SURFACE_TYPE_EMU:
                surface_type = param;
                break;

            // Note: If this param is duplicated, it will break. Well, that's the user's problem right..
            case EGL_RED_SIZE_EMU:
            case EGL_GREEN_SIZE_EMU:
            case EGL_BLUE_SIZE_EMU:
            case EGL_ALPHA_SIZE_EMU:
                if (total_color_bits_calculated == -1) total_color_bits_calculated = param;
                else total_color_bits_calculated += param;

                break;

            case EGL_BUFFER_SIZE_EMU:
                total_color_bits_provided = param;
                break;

            case EGL_RENDERABLE_TYPE_EMU:
                gles_context_type = param;
                break;

            default:
                break;
            }
        }

        if (total_color_bits_provided == -1) {
            if (total_color_bits_calculated != -1) {
                total_color_bits_provided = total_color_bits_calculated;
            }
        }

        egl_config::surface_type type;
        if (surface_type == -1) {
            type = egl_config::EGL_SURFACE_TYPE_WINDOW;
        } else {
            switch (surface_type) {
            case EGL_PBUFFER_BIT_EMU:
                type = egl_config::EGL_SURFACE_TYPE_PBUFFER;
                break;

            case EGL_WINDOW_BIT_EMU:
                type = egl_config::EGL_SURFACE_TYPE_WINDOW;
                break;

            default:
                // This is not a fail. We don't support it now.
                *num_config_choosen = 0;
                return EGL_TRUE;
            }
        }

        std::uint32_t current_fill_index = 0;

        for (std::int32_t i = 0; i < EGL_EMU_CONFIG_LIST_VALS_LENGTH; i++) {
            egl_config config_parser(EGL_EMU_CONFIG_LIST_VALS[i]);
            if (config_parser.get_surface_type() == type) {
                egl_config::target_context_version ver = config_parser.get_target_context_version();
                std::uint32_t egl_version_enum = EGL_OPENGL_ES1_BIT;

                if (ver == egl_config::EGL_TARGET_CONTEXT_ES2) {
                    egl_version_enum = EGL_OPENGL_ES2_BIT;
                }

                if (gles_context_type & egl_version_enum) {
                    if ((total_color_bits_provided == -1) || (config_parser.buffer_size() >= total_color_bits_provided)) {
                        if (current_fill_index < config_array_size) {
                            configs[current_fill_index++] = config_parser;
                        }
                    }
                }
            }
        }

        *num_config_choosen = current_fill_index;
        return EGL_TRUE;
    }

    BRIDGE_FUNC_LIBRARY(egl_surface_handle, egl_create_window_surface_emu, egl_display display, std::uint32_t choosen_config_value, utils::window_client_interface *win, std::int32_t *additional_attribs) {
        drivers::graphics_driver *driver = sys->get_graphics_driver();
        kernel_system *kern = sys->get_kernel_system();
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_config choosen_config(choosen_config_value);

        if (choosen_config.get_surface_type() != egl_config::EGL_SURFACE_TYPE_WINDOW) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        if (choosen_config.buffer_size() > 32) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        utils::window_session *ss = win->get_window_session(kern->crr_process());
        if (!ss) {
            egl_push_error(sys, EGL_BAD_NATIVE_WINDOW_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        service::session *corresponded_session = kern->get<service::session>(ss->session_handle_);
        if (!corresponded_session) {
            egl_push_error(sys, EGL_BAD_NATIVE_WINDOW_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        epoc::window_server_client *wclient = dp->winserv_->get_client(corresponded_session->unique_id());
        if (!wclient) {
            egl_push_error(sys, EGL_BAD_NATIVE_WINDOW_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        epoc::canvas_base *canvas = reinterpret_cast<epoc::canvas_base*>(wclient->get_object(win->handle_));
        if (!canvas || (canvas->type != epoc::window_kind::client)) {
            egl_push_error(sys, EGL_BAD_NATIVE_WINDOW_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        drivers::handle hh = drivers::create_bitmap(driver, canvas->size_for_egl_surface() * canvas->scr->display_scale_factor, choosen_config.buffer_size());
        if (hh == 0) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        std::unique_ptr<egl_surface> result_surface = std::make_unique<egl_surface>(canvas, canvas->scr, canvas->size_for_egl_surface(), hh, choosen_config);
        egl_surface_handle result_handle = controller.add_managed_surface(result_surface);

        if (result_handle == EGL_NO_SURFACE_EMU) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        return result_handle;
    }

    BRIDGE_FUNC_LIBRARY(egl_surface_handle, egl_create_pbuffer_surface_emu, egl_display display, std::uint32_t choosen_config_value, std::int32_t *additional_attribs) {
        if (!additional_attribs) {
            // I... Need .. To .. know .. the ... width... and ... weight .. of the surface !!!
            // ps: (given up on the three dots at the end)
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        egl_config choosen_config(choosen_config_value);

        if (choosen_config.get_surface_type() != egl_config::EGL_SURFACE_TYPE_PBUFFER) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        if (choosen_config.buffer_size() > 32) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        epoc::screen *backed_screen = dp->winserv_->get_screen(display - 1);
        if (!backed_screen) {
            egl_push_error(sys, EGL_BAD_DISPLAY_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        eka2l1::vec2 dim(0, 0);
        while (true) {
            std::int32_t pname = *additional_attribs++;
            if (pname == 0) {
                break;
            }
            std::int32_t param = *additional_attribs++;
            switch (pname) {
            case EGL_WIDTH_EMU:
                dim.x = param;
                break;

            case EGL_HEIGHT_EMU:
                dim.y = param;
                break;

            default:
                break;
            }
        }

        if ((dim.x == 0) || (dim.y == 0) || (dim.x > 2048) || (dim.y > 2048)) {
            egl_push_error(sys, EGL_BAD_ATTRIBUTE_EMU);
            return EGL_NO_SURFACE_EMU;
        }

        drivers::graphics_driver *driver = sys->get_graphics_driver();

        drivers::handle hh = drivers::create_bitmap(driver, dim, choosen_config.buffer_size());
        if (hh == 0) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        std::unique_ptr<egl_surface> result_surface = std::make_unique<egl_surface>(nullptr, backed_screen, dim, hh, choosen_config);

        result_surface->current_scale_ = 1.0f;
        egl_surface_handle result_handle = controller.add_managed_surface(result_surface);

        if (result_handle == EGL_NO_SURFACE_EMU) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        return result_handle;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_context_handle, egl_create_context_emu, egl_display display, std::uint32_t choosen_config_value, egl_context_handle shared_context, std::int32_t *additional_attribs_) {
        if (shared_context) {
            LOG_ERROR(HLE_DISPATCHER, "Shared context is not yet supported!");
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);

            return EGL_NO_CONTEXT_EMU;
        }

        egl_config choosen_config(choosen_config_value);
        egl_context_instance context_inst = nullptr;

        switch (choosen_config.get_target_context_version()) {
        case egl_config::EGL_TARGET_CONTEXT_ES11:
            context_inst = std::make_unique<egl_context_es1>();
            break;

        case egl_config::EGL_TARGET_CONTEXT_ES2:
            context_inst = std::make_unique<egl_context_es2>();
            break;

        default:
            LOG_ERROR(HLE_DISPATCHER, "Context other than ES 1.1 is not yet supported!");
            egl_push_error(sys, EGL_BAD_CONFIG);

            return EGL_NO_CONTEXT_EMU;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_context_handle hh = controller.add_context(context_inst);
        if (!hh) {
            LOG_ERROR(HLE_DISPATCHER, "Fail to add GLES context to management!");
            egl_push_error(sys, EGL_BAD_CONFIG);

            return EGL_NO_CONTEXT_EMU;
        }

        return hh;
    }

    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_make_current_emu, egl_display display, egl_surface_handle read_surface, egl_surface_handle write_surface, egl_context_handle context) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        kernel_system *kern = sys->get_kernel_system();

        egl_surface *read_surface_real = controller.get_managed_surface(read_surface);
        egl_surface *write_surface_real = controller.get_managed_surface(write_surface);

        egl_context *context_real = controller.get_context(context);

        if (!controller.make_current(kern->crr_thread()->unique_id(), context)) {
            egl_push_error(sys, EGL_BAD_CONTEXT_EMU);
            return EGL_FALSE;
        }

        if (context_real) {
            if (context_real->read_surface_ != read_surface_real) {
                if (context_real->read_surface_) {
                    context_real->read_surface_->bounded_context_ = nullptr;

                    if (context_real->read_surface_->dead_pending_) {
                        context_real->cmd_builder_.destroy_bitmap(context_real->read_surface_->handle_);
                        controller.remove_managed_surface_from_management(context_real->read_surface_);
                    }
                }
            }

            if (context_real->draw_surface_ != write_surface_real) {
                if (context_real->draw_surface_) {
                    context_real->draw_surface_->bounded_context_ = nullptr;
                    if ((context_real->read_surface_ != context_real->draw_surface_) && (context_real->draw_surface_->dead_pending_))
                        context_real->cmd_builder_.destroy_bitmap(context_real->draw_surface_->handle_);
                        controller.remove_managed_surface_from_management(context_real->draw_surface_);
                }
            }

            egl_surface *prev_read = context_real->read_surface_;
            egl_surface *prev_write = context_real->draw_surface_;

            context_real->read_surface_ = read_surface_real;
            context_real->draw_surface_ = write_surface_real;
            context_real->read_surface_handle_ = read_surface;
            context_real->draw_surface_handle_ = write_surface;

            context_real->on_surface_changed(prev_read, prev_write);

            read_surface_real->bounded_context_ = context_real;
            write_surface_real->bounded_context_ = context_real;

            context_real->cmd_builder_.bind_bitmap(write_surface_real->handle_, read_surface_real->handle_);
        }

        return EGL_TRUE;
    }

    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_destroy_context_emu, egl_display display, egl_context_handle handle) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        
        controller.remove_context(handle);
        return EGL_TRUE;
    }

    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_destroy_surface_emu, egl_display display, egl_surface_handle handle) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        controller.destroy_managed_surface(handle);
        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_swap_buffers_emu, egl_display display, egl_surface_handle handle) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_surface *surface = controller.get_managed_surface(handle);
        if (!surface) {
            egl_push_error(sys, EGL_BAD_SURFACE_EMU);
            return EGL_FALSE;
        }

        drivers::graphics_driver *drv = sys->get_graphics_driver();

        if (surface->backed_window_) {
            egl_context *ctx = surface->bounded_context_;
            if (surface->current_scale_ != surface->backed_screen_->display_scale_factor) {
                // Silently resize and scale
                float new_display_factor = surface->backed_screen_->display_scale_factor;
                eka2l1::vec2 new_scaled_size = surface->dimension_ * new_display_factor;

                drivers::handle new_surface = drivers::create_bitmap(drv, new_scaled_size, 32);

                ctx->cmd_builder_.bind_bitmap(new_surface);
                ctx->cmd_builder_.draw_bitmap(surface->handle_, 0, eka2l1::rect(eka2l1::vec2(0, 0), new_scaled_size),
                    eka2l1::rect(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0)));
                ctx->cmd_builder_.destroy_bitmap(surface->handle_);

                surface->handle_ = new_surface;
                surface->current_scale_ = surface->backed_screen_->display_scale_factor;
            }

            if (ctx && surface->backed_window_->can_be_physically_seen()) {
                drivers::graphics_command_builder &window_builder = surface->backed_window_->driver_builder_;
                window_builder.set_feature(drivers::graphics_feature::blend, false);
                window_builder.set_feature(drivers::graphics_feature::depth_test, false);

                eka2l1::rect dest_rect = surface->backed_window_->abs_rect;
                dest_rect.scale(surface->backed_screen_->display_scale_factor);

                int rotation = 0;

                if (surface->backed_window_->flags & epoc::window::flag_fix_native_orientation) {
                    // Surface is also upside down. So a 180 flip :(
                    rotation = (surface->backed_screen_->current_mode().rotation + 180) % 360;
                    eka2l1::drivers::advance_draw_pos_around_origin(dest_rect, rotation);

                    if (rotation % 180 != 0) {
                        std::swap(dest_rect.size.x, dest_rect.size.y);
                    }
                }

                window_builder.draw_bitmap(surface->handle_, 0, dest_rect, eka2l1::rect(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0)),
                    eka2l1::vec2(0, 0), static_cast<float>(rotation), drivers::bitmap_draw_flag_flip);

                surface->backed_window_->content_changed(true);
            }
        }

        if (surface->bounded_context_) {
            surface->bounded_context_->flush_to_driver(drv, true);
        }

        if (surface->backed_window_)
            surface->backed_window_->try_update(sys->get_kernel_system()->crr_thread());

        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_query_surface_emu, egl_display display, egl_surface_handle handle, std::uint32_t attribute, std::int32_t *value) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_surface *surface = controller.get_managed_surface(handle);
        if (!surface) {
            egl_push_error(sys, EGL_BAD_SURFACE_EMU);
            return EGL_FALSE;
        }

        if (!value) {
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
            return EGL_FALSE;
        }

        switch (attribute) {
        case EGL_WIDTH_EMU:
            *value = surface->dimension_.x;
            break;

        case EGL_HEIGHT_EMU:
            *value = surface->dimension_.y;
            break;

        default:
            LOG_ERROR(HLE_DISPATCHER, "Unrecognised/unhandled attribute to query 0x{:X}", attribute);
            egl_push_error(sys, EGL_BAD_ATTRIBUTE_EMU);

            return EGL_FALSE;
        }

        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(std::int32_t, egl_wait_native_emu, std::int32_t engine) {
        // Nothing
        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(std::int32_t, egl_wait_gl_emu) {
        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(address, egl_query_string_emu, egl_display display, std::uint32_t param) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((param != EGL_EXTENSIONS_EMU) && (param != EGL_VENDOR_EMU) && (param != EGL_VERSION_EMU)) {
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
            return 0;
        }

        address res = dp->retrieve_static_string(param);
        if (res == 0) {
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
        }
        return res;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_get_config_attrib_emu, egl_display display, std::uint32_t config, std::uint32_t attribute, std::uint32_t *value) {
        if (!value) {
            egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
            return 0;
        }

        egl_config parser(config);

        switch (attribute) {
        case EGL_RENDERABLE_TYPE_EMU:
            if (parser.get_target_context_version() == egl_config::EGL_TARGET_CONTEXT_ES11) {
                *value = EGL_OPENGL_ES1_BIT;
            } else {
                *value = EGL_OPENGL_ES2_BIT;
            }

            break;

        case EGL_RED_SIZE_EMU:
            *value = parser.red_bits();
            break;
            
        case EGL_GREEN_SIZE_EMU:
            *value = parser.green_bits();
            break;

        case EGL_BLUE_SIZE_EMU:
            *value = parser.blue_bits();
            break;

        case EGL_ALPHA_SIZE_EMU:
            *value = parser.alpha_bits();
            break;

        case EGL_CONFIG_CAVEAT_EMU:
            *value = EGL_NONE_EMU;
            break;

        // Depth stencil surface is in D24S8 format.
        case EGL_DEPTH_SIZE_EMU:
            *value = 24;
            break;

        case EGL_STENCIL_SIZE_EMU:
            *value = 8;
            break;
        
        case EGL_SURFACE_TYPE_EMU:
            switch (parser.get_surface_type()) {
            case egl_config::EGL_SURFACE_TYPE_PBUFFER:
                *value = EGL_PBUFFER_BIT_EMU;
                break;

            case egl_config::EGL_SURFACE_TYPE_WINDOW:
                *value = EGL_WINDOW_BIT_EMU;
                break;

            default:
                *value = EGL_WINDOW_BIT_EMU;
                break;
            }

            break;

        case EGL_MAX_PBUFFER_WIDTH_EMU:
            *value = MAX_EGL_FB_WIDTH;
            break;

        case EGL_MAX_PBUFFER_HEIGHT_EMU:
            *value = MAX_EGL_FB_HEIGHT;
            break;

        default:
            LOG_ERROR(HLE_DISPATCHER, "Unhandled config attribute getter 0x{:X}", attribute);
            egl_push_error(sys, EGL_BAD_ATTRIBUTE_EMU);

            return 0;
        }

        return 1;
    }
    
    BRIDGE_FUNC_LIBRARY(egl_context_handle, egl_get_current_context_emu) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_context *crr = controller.current_context(sys->get_kernel_system()->crr_thread()->unique_id());
        if (!crr) {
            return EGL_NO_CONTEXT_EMU;
        }

        return crr->my_id_;
    }

    BRIDGE_FUNC_LIBRARY(egl_display, egl_get_current_display_emu) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_context *crr = controller.current_context(sys->get_kernel_system()->crr_thread()->unique_id());
        if (!crr) {
            return 0;
        }

        if (crr->draw_surface_) {
            return static_cast<egl_display>(crr->draw_surface_->backed_screen_->number + 1);
        }

        return 0;
    }

    BRIDGE_FUNC_LIBRARY(egl_surface_handle, egl_get_current_surface_emu, std::uint32_t which) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        egl_context *crr = controller.current_context(sys->get_kernel_system()->crr_thread()->unique_id());
        if (!crr) {
            return 0;
        }

        if (which == EGL_DRAW_EMU) {
            return crr->draw_surface_handle_;
        }

        if (which == EGL_READ_EMU) {
            return crr->read_surface_handle_;
        }

        egl_push_error(sys, EGL_BAD_PARAMETER_EMU);
        return 0;
    }

    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_copy_buffers_emu, egl_display display, egl_surface_handle handle, void *native_pixmap) {
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!native_pixmap) {
            egl_push_error(sys, EGL_BAD_NATIVE_PIXMAP_EMU);
            return EGL_FALSE;
        }

        egl_surface *surface = controller.get_managed_surface(handle);
        if (!surface) {
            egl_push_error(sys, EGL_BAD_SURFACE_EMU);
            return EGL_FALSE;
        }

        // Try to grab the bitwise bitmap
        kernel_system *kern = sys->get_kernel_system();
        fbs_server *fbss = kern->get_by_name<fbs_server>(epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version()));

        utils::fbs_bitmap *guest_bmp_ptr = reinterpret_cast<utils::fbs_bitmap*>(native_pixmap);
        epoc::bitwise_bitmap *bbmp = guest_bmp_ptr->bitwise_bitmap_addr_.cast<epoc::bitwise_bitmap>().get(kern->crr_process());

        if (!bbmp) {
            egl_push_error(sys, EGL_BAD_NATIVE_PIXMAP_EMU);
            return EGL_FALSE;
        }

        std::uint8_t *bbmp_data_ptr = bbmp->data_pointer(fbss);
        if (!bbmp_data_ptr) {
            egl_push_error(sys, EGL_BAD_NATIVE_PIXMAP_EMU);
            return EGL_FALSE;
        }

        drivers::graphics_driver *drv = sys->get_graphics_driver();
        if (surface->bounded_context_) {
            surface->bounded_context_->flush_to_driver(drv, true);
        }

        eka2l1::object_size size_to_read = bbmp->header_.size_pixels;

        if (surface->dimension_.x < size_to_read.y) {
            size_to_read.x = surface->dimension_.x;
        }
        if (surface->dimension_.y < size_to_read.y) {
            size_to_read.y = surface->dimension_.y;
        }

        if (!drivers::read_bitmap(drv, surface->handle_, eka2l1::point(0, 0), size_to_read, bbmp->header_.bit_per_pixels,
            bbmp_data_ptr)) {
            LOG_ERROR(HLE_DISPATCHER, "Failed to syncrhonize EGL bitmap data to guest bitmap!");
            return EGL_FALSE;
        }

        // TODO: Make a surface cache associated with the bitmap address, so that when GDI calls try to 
        // draw these, we can use it. Maybe also for upscaling.
        std::uint32_t byte_width = bbmp->byte_width_;

        // Symbian bitmap stores things a bit different. Need to flip. Plus maybe color transformation
        switch (bbmp->header_.bit_per_pixels) {
        case 32: {
            std::uint32_t *data_u32 = reinterpret_cast<std::uint32_t*>(bbmp_data_ptr);
            std::uint32_t word_width = byte_width / 4;
            for (int lc = 0; lc < size_to_read.y / 2; lc++) {
                for (int x = 0; x < size_to_read.x; x++) {
                    int temp = (data_u32[lc * word_width + x] & 0xFF00FF00) | ((data_u32[lc * word_width + x] & 0xFF) << 16)
                        | (((data_u32[lc * word_width + x] & 0xFF0000) >> 16));

                    int revl = (size_to_read.y - lc - 1);

                    data_u32[lc * word_width + x] = (data_u32[revl * word_width + x] & 0xFF00FF00) | ((data_u32[revl * word_width + x] & 0xFF) << 16)
                        | (((data_u32[revl * word_width + x] & 0xFF0000) >> 16));

                    data_u32[revl * word_width + x] = temp;
                }
            }

            if (size_to_read.y & 1) {
                int y = size_to_read.y / 2;
                for (int x = 0; x < size_to_read.x; x++) {
                    data_u32[y * word_width + x] = (data_u32[y * word_width + x] & 0xFF00FF00) | ((data_u32[y * word_width + x] & 0xFF) << 16) |
                        (((data_u32[y * word_width + x] & 0xFF0000) >> 16));
                }
            }

            break;
        }

        default:
            LOG_WARN(HLE_DISPATCHER, "Unhandle FBSBMP transformation from GLES1 surface for bpp={}", bbmp->header_.bit_per_pixels);
            break;
        }

        return EGL_TRUE;
    }
    
    BRIDGE_FUNC_LIBRARY(address, egl_get_proc_address_emu, const char *procname) {
        // Safe check: Make sure it's not searching for anything else other than GL and EGL
        if (!procname) {
            return 0;
        }

        if ((strncmp(procname, "egl", 3) != 0) && (strncmp(procname, "gl", 2) != 0)) {
            return 0;
        }

        return sys->get_dispatcher()->lookup_dispatcher_function_by_symbol(procname);
    }
}