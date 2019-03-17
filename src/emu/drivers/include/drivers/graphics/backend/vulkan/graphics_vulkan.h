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

#pragma once

#include <drivers/graphics/backend/graphics_driver_shared.h>
#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif EKA2L1_PLATFORM(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif EKA2L1_PLATFORM(UNIX)
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.hpp>

namespace eka2l1::drivers {
    class vulkan_graphics_driver: public shared_graphics_driver {
        vk::UniqueInstance inst_;
        vk::UniqueDebugReportCallbackEXT reporter_;
        
        PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback_ext_;
        PFN_vkDestroyDebugReportCallbackEXT destroy_debug_report_callback_ext_;

        void create_instance();
        void create_debug_callback();

    public:
        explicit vulkan_graphics_driver(const vec2 &scr);
    };
}
