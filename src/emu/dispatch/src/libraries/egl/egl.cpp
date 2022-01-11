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
#include <dispatch/dispatcher.h>
#include <kernel/kernel.h>

#include <system/epoc.h>
#include <services/window/window.h>
#include <services/window/classes/winuser.h>

#include <drivers/itc.h>
#include <drivers/graphics/graphics.h>

namespace eka2l1::dispatch {
    // First bit is surface type, and second bit is buffer size bits
    static constexpr std::uint32_t EGL_EMU_CONFIG_LIST_VALS[] = {
        // GLES1
        0b010, 0b100, 0b110,
        0b011, 0b101, 0b111,
        // GLES2
        0b1010, 0b1100, 0b1110,
        0b1011, 0b1101, 0b1111,
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
                if ((total_color_bits_provided == -1) || (config_parser.buffer_size() >= total_color_bits_provided)) {
                    if (current_fill_index < config_array_size) {
                        configs[current_fill_index++] = config_parser;
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

        drivers::handle hh = drivers::create_bitmap(driver, canvas->size(), choosen_config.buffer_size());
        if (hh == 0) {
            egl_push_error(sys, EGL_BAD_CONFIG);
            return EGL_NO_SURFACE_EMU;
        }

        std::unique_ptr<egl_surface> result_surface = std::make_unique<egl_surface>(canvas,
            canvas->scr, canvas->size(), hh, egl_config::EGL_SURFACE_TYPE_WINDOW);

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

        std::unique_ptr<egl_surface> result_surface = std::make_unique<egl_surface>(nullptr,
            backed_screen, dim, hh, egl_config::EGL_SURFACE_TYPE_PBUFFER);

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
            if (ctx) {
                if (!surface->backed_window_->driver_win_id) {
                    surface->backed_window_->driver_win_id = drivers::create_bitmap(drv, surface->backed_window_->size(), 32);
                }

                eka2l1::rect draw_rect(eka2l1::vec2(0, 0), surface->dimension_);

                ctx->cmd_builder_.set_feature(drivers::graphics_feature::clipping, false);
                ctx->cmd_builder_.set_feature(drivers::graphics_feature::depth_test, false);
                ctx->cmd_builder_.set_feature(drivers::graphics_feature::cull, false);
                ctx->cmd_builder_.set_feature(drivers::graphics_feature::blend, false);

                ctx->cmd_builder_.bind_bitmap(surface->backed_window_->driver_win_id);
                ctx->cmd_builder_.draw_bitmap(surface->handle_, 0, draw_rect, draw_rect, eka2l1::vec2(0, 0), 0.0f, drivers::bitmap_draw_flag_no_flip);

                surface->backed_window_->has_redraw_content(true);
            }
        }

        if (surface->bounded_context_) {
            drivers::command_list retrieved = surface->bounded_context_->cmd_builder_.retrieve_command_list();
            drv->submit_command_list(retrieved);

            surface->bounded_context_->init_context_state();
        }

        if (surface->backed_window_)
            surface->backed_window_->take_action_on_change(sys->get_kernel_system()->crr_thread());

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
}