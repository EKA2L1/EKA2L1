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

#include <common/configure.h>

#ifdef BUILD_WITH_VULKAN

#include <drivers/graphics/backend/vulkan/graphics_vulkan.h>
#include <common/log.h>
#include <common/platform.h>
#include <vector>

namespace eka2l1::drivers {
    static VkBool32 vulkan_reporter(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT /*objectType*/, uint64_t /*object*/, size_t /*location*/,
        int32_t /*messageCode*/, const char* /*pLayerPrefix*/, const char* pMessage, void* /*pUserData*/) {
        switch (flags) {
        case VK_DEBUG_REPORT_INFORMATION_BIT_EXT: {
            LOG_INFO("{}", pMessage);
            break;
        }

        case VK_DEBUG_REPORT_WARNING_BIT_EXT: {
            LOG_WARN("{}", pMessage);
            break;
        }

        case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT: {
            LOG_WARN("Performance: {}", pMessage);
            break;
        }

        case VK_DEBUG_REPORT_ERROR_BIT_EXT: {
            LOG_ERROR("{}", pMessage);
            break;
        }

        case VK_DEBUG_REPORT_DEBUG_BIT_EXT: {
            LOG_TRACE("{}", pMessage);
            break;
        }

        default: {
            LOG_INFO("{}", pMessage);
            break;
        }
        }

        return true;
    }

    void vulkan_graphics_driver::create_instance() {
        std::vector<const char*> enabled_layers;
        // Enable validations
        enabled_layers.push_back("VK_LAYER_LUNARG_standard_validation");

        // Enable surface extensions
        std::vector<const char*> enabled_extensions;
        enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        
#if EKA2L1_PLATFORM(WIN32)
        enabled_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif EKA2L1_PLATFORM(ANDROID)
        enabled_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif EKA2L1_PLATFORM(UNIX)
        enabled_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

        vk::ApplicationInfo app_info("EKA2L1", 1, "EDriver", 1, VK_API_VERSION_1_1);
        vk::InstanceCreateInfo instance_create_info({}, &app_info, static_cast<std::uint32_t>(enabled_layers.size()),
            enabled_layers.data(), static_cast<std::uint32_t>(enabled_extensions.size()), enabled_extensions.data());
        
        inst_ = vk::createInstanceUnique(instance_create_info);

        // We should also get validation functions
        create_debug_report_callback_ext_ = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(inst_->getProcAddr("vkCreateDebugReportCallbackEXT"));
        destroy_debug_report_callback_ext_ = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(inst_->getProcAddr("vkDestroyDebugReportCallbackEXT")); 
    }

    void vulkan_graphics_driver::create_debug_callback() {
        vk::DebugReportFlagsEXT report_callback_flags(vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | 
            vk::DebugReportFlagBitsEXT::eError);
        vk::DebugReportCallbackCreateInfoEXT report_callback_create_info(report_callback_flags, vulkan_reporter);

        reporter_ = inst_->createDebugReportCallbackEXTUnique(report_callback_create_info);
    }
    
    vulkan_graphics_driver::vulkan_graphics_driver(const vec2 &scr) {
        create_instance();
        create_debug_callback();
    }
}

#endif
