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

PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback_ext_;
PFN_vkDestroyDebugReportCallbackEXT destroy_debug_report_callback_ext_;

VkResult vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, 
    VkDebugReportCallbackEXT* pCallback) {
    return create_debug_report_callback_ext_(instance, pCreateInfo, pAllocator, pCallback);
}

void vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator) {
    destroy_debug_report_callback_ext_(instance, callback, pAllocator);
}

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

    bool vulkan_graphics_driver::create_instance() {
        auto avail_layers = vk::enumerateInstanceLayerProperties();

        std::vector<const char*> enabled_layers;

        auto add_layer_if_avail = [&](const char *name) -> bool {
            for (auto &avail_layer: avail_layers) {
                if (strncmp(avail_layer.layerName, name, strlen(name)) == 0) {
                    enabled_layers.push_back(name);
                    return true;
                }
            }

            return false;
        };

        // Enable layers
        if (!add_layer_if_avail("VK_LAYER_LUNARG_standard_validation")) {
            bool result = false;

            result = add_layer_if_avail("VK_LAYER_GOOGLE_threading");
            result = add_layer_if_avail("VK_LAYER_LUNARG_parameter_validation");
            result = add_layer_if_avail("VK_LAYER_LUNARG_object_tracker");
            result = add_layer_if_avail("VK_LAYER_LUNARG_core_validation");
            result = add_layer_if_avail("VK_LAYER_GOOGLE_unique_objects");

            if (!result) {
                LOG_ERROR("Instance can't be created, needed validation layers not found!");
                return false;
            }
        }

        // Enable surface extensions
        std::vector<const char*> enabled_extensions;
        enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        enabled_extensions.push_back("VK_EXT_debug_report");
        enabled_extensions.push_back("VK_EXT_debug_utils");
        
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
        
        try {
            inst_ = vk::createInstanceUnique(instance_create_info);
        } catch (std::exception &e) {
            LOG_ERROR("Instance creation failed with error: {}", e.what());
            return false;
        }

        if (!inst_) {
            return false;
        }

        return true;
    }

    bool vulkan_graphics_driver::create_debug_callback() {
        // We should also get validation functions
        create_debug_report_callback_ext_ = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(inst_->getProcAddr("vkCreateDebugReportCallbackEXT"));
        destroy_debug_report_callback_ext_ = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(inst_->getProcAddr("vkDestroyDebugReportCallbackEXT"));

        if (!create_debug_report_callback_ext_ || !destroy_debug_report_callback_ext_) {
            return false;
        }

        vk::DebugReportFlagsEXT report_callback_flags(vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | 
            vk::DebugReportFlagBitsEXT::eError);
        vk::DebugReportCallbackCreateInfoEXT report_callback_create_info(report_callback_flags, vulkan_reporter);

        reporter_ = inst_->createDebugReportCallbackEXTUnique(report_callback_create_info);

        if (!reporter_) {
            return false;
        }

        return true;
    }
    
    static std::uint64_t score_for_me_the_gpu(const vk::PhysicalDevice &dvc) {
        std::uint64_t scr = 0;

        auto prop = dvc.getProperties();
        
        switch (prop.deviceType) {
        // Prefer discrete GPU, not integrated.
        case vk::PhysicalDeviceType::eDiscreteGpu: {
            scr += 1000;
            break;
        }

        default: {
            scr += 500;
            break;
        }
        }

        LOG_TRACE("Found device: {}, score: {}", prop.deviceName, scr);

        return scr;
    }

    bool vulkan_graphics_driver::create_device() {
        {
            auto dvcs = inst_->enumeratePhysicalDevices();
            if (dvcs.size() == 0) {
                LOG_ERROR("No physical devices found for Vulkan!");
                return false;
            }

            std::uint64_t last_max_dvc_score = 0;
            std::size_t idx = 0;

            for (std::size_t i = 0; i < dvcs.size(); i++) {
                const std::uint64_t scr = score_for_me_the_gpu(dvcs[i]);
                if (scr > last_max_dvc_score) {
                    last_max_dvc_score = scr;
                    idx = i;
                }
            }

            phys_dvc_ = std::move(dvcs[idx]);
            LOG_TRACE("Choosing device: {}", phys_dvc_.getProperties().deviceName);
        }

        std::uint32_t queue_index = 0xFFFF;

        // Get the queue that supports presenting our surface
        auto fam_queues = phys_dvc_.getQueueFamilyProperties();
        std::vector<std::size_t> support_presenting;

        for (std::size_t i = 0; i < fam_queues.size(); i++) {
            if (phys_dvc_.getSurfaceSupportKHR(static_cast<std::uint32_t>(i), surface_.get())) {
                support_presenting.push_back(i);
            }
        }

        // We need to find the graphics queue for this physical device
        // Iterate through queue that currently support presenting, check if those also supports graphics.
        for (std::size_t i = 0; i < support_presenting.size(); i++) {
            if (fam_queues[support_presenting[i]].queueFlags & vk::QueueFlagBits::eGraphics) {
                queue_index = static_cast<std::uint32_t>(i);
                break;
            }
        }

        // If we still can't find the queue that satisfy our condition (both support present and graphics, fallback to only graphics)
        if (queue_index == 0xFFFF) {
            for (std::size_t i = 0; i < fam_queues.size(); i++) {
                if (fam_queues[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                    queue_index = static_cast<std::uint32_t>(i);
                    break;
                }
            }
        }

        // If no queue satisfy that last test, we failed. Bye!
        if (queue_index == 0xFFFF) {
            LOG_ERROR("No queue presents support graphics. Abort");
            return false;
        }

        float queue_pris[1] = {0.0f};
        vk::DeviceQueueCreateInfo queue_create_info(vk::DeviceQueueCreateFlags{}, queue_index, 1, queue_pris);
        vk::DeviceCreateInfo device_create_info(vk::DeviceCreateFlags{}, 1, &queue_create_info);

        dvc_ = phys_dvc_.createDeviceUnique(device_create_info);

        if (!dvc_) {
            return false;
        }

        return true;
    }

    bool vulkan_graphics_driver::create_surface() {
#if EKA2L1_PLATFORM(WIN32)
        vk::Win32SurfaceCreateInfoKHR surface_create_info(vk::Win32SurfaceCreateFlagsKHR {}, nullptr, reinterpret_cast<HWND>(native_win_handle_));
        try {
            surface_ = inst_->createWin32SurfaceKHRUnique(surface_create_info);
        } catch (std::exception &ex) {
            LOG_ERROR("Create Win32 Vulkan surface failed with error: {}", ex.what());
            return false;
        }

        return true;
#elif EKA2L1_PLATFORM(ANDROID)
        vk::AndroidSurfaceCreateInfoKHR surface_create_info(vk::AndroidSurfaceCreateFlagsKHR{}, reinterpret_cast<struct ANativeWindow*>(native_win_handle_));
        try {
            surface_ = inst_->createAndroidSurfaceKHRUnique(surface_create_info);
        } catch (std::exception &ex) {
            LOG_ERROR("Create Android Vulkan surface failed with error: {}", ex.what());
            return false;
        }
#else
        vk::XcbSurfaceCreateInfoKHR surface_create_info(vk::XcbSurfaceCreateFlagsKHR {}, nullptr, reinterpret_cast<xcb_window_t>(native_win_handle_));
        try {
            surface_ = inst_->createXcbSurfaceKHRUnique(surface_create_info);
        } catch (std::exception &ex) {
            LOG_ERROR("Create XCB Vulkan surface failed with error: {}", ex.what());
            return false;
        }
#endif

        return true;
    }

    bool vulkan_graphics_driver::create_swapchain() {
        // Get supported format by surface
        return true;
    }
    
    vulkan_graphics_driver::vulkan_graphics_driver(const vec2 &scr, void *native_win_handle)
        : native_win_handle_(native_win_handle) {
        create_instance();
        create_debug_callback();
        create_surface();
        create_device();
    }

    vulkan_graphics_driver::~vulkan_graphics_driver() {
        // shared_graphics_driver::~shared_graphics_driver();
    }
}

#endif
