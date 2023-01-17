/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <dispatch/libraries/egl/def.h>
#include <dispatch/def.h>

#include <utils/guest/window.h>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_LIBRARY(egl_display, egl_get_display_emu, std::int32_t display_index);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_initialize_emu, egl_display display, std::int32_t *major, std::int32_t *minor);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_get_configs_emu, egl_display display, egl_config *configs, std::int32_t config_array_size, std::int32_t *config_total_size);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_choose_config_emu, egl_display display, std::int32_t *attrib_lists, egl_config *configs, std::int32_t config_array_size, std::int32_t *num_config_choosen);
    BRIDGE_FUNC_LIBRARY(egl_surface_handle, egl_create_window_surface_emu, egl_display display, std::uint32_t choosen_config_value, utils::window_client_interface *win, std::int32_t *additional_attribs);
    BRIDGE_FUNC_LIBRARY(egl_surface_handle, egl_create_pbuffer_surface_emu, egl_display display, std::uint32_t choosen_config_value, std::int32_t *additional_attribs);
    BRIDGE_FUNC_LIBRARY(egl_context_handle, egl_create_context_emu, egl_display display, std::uint32_t choosen_config_value, egl_context_handle shared_context, std::int32_t *additional_attribs_);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_make_current_emu, egl_display display, egl_surface_handle read_surface, egl_surface_handle write_surface, egl_context_handle context);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_destroy_context_emu, egl_display display, egl_context_handle handle);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_destroy_surface_emu, egl_display display, egl_surface_handle handle);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_swap_buffers_emu, egl_display display, egl_surface_handle handle);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_query_surface_emu, egl_display display, egl_surface_handle handle, std::uint32_t attribute, std::int32_t *value);
    BRIDGE_FUNC_LIBRARY(std::int32_t, egl_get_error);
    BRIDGE_FUNC_LIBRARY(std::int32_t, egl_wait_native_emu, std::int32_t engine);
    BRIDGE_FUNC_LIBRARY(std::int32_t, egl_wait_gl_emu);
    BRIDGE_FUNC_LIBRARY(address, egl_query_string_emu, egl_display display, std::uint32_t param);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_get_config_attrib_emu, egl_display display, std::uint32_t config, std::uint32_t attribute, std::uint32_t *value);
    BRIDGE_FUNC_LIBRARY(egl_context_handle, egl_get_current_context_emu);
    BRIDGE_FUNC_LIBRARY(egl_display, egl_get_current_display_emu);
    BRIDGE_FUNC_LIBRARY(egl_surface_handle, egl_get_current_surface_emu, std::uint32_t which);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_copy_buffers_emu, egl_display display, egl_surface_handle handle, void *native_pixmap);
    BRIDGE_FUNC_LIBRARY(address, egl_get_proc_address_emu, const char *procname);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_bind_api_emu, const std::uint32_t bind_api);
    BRIDGE_FUNC_LIBRARY(egl_boolean, egl_surface_attrib_emu, egl_display display, egl_surface_handle read_surface,
        std::int32_t attribute, std::int32_t value);
}