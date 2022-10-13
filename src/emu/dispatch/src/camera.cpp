/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <drivers/camera/camera_collection.h>

#include <dispatch/dispatcher.h>
#include <dispatch/camera.h>
#include <common/log.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <kernel/kernel.h>
#include <kernel/thread.h>

#include <cstring>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_number_of_cameras) {
        return static_cast<std::int32_t>(drivers::camera::get_collection()->count());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_create, std::int32_t index, drivers::camera::info *cam_info) {
        if (index < 0) {
            LOG_ERROR(HLE_DISPATCHER, "Camera index requested for creation is negative!");
            return epoc::error_argument;
        }

        if (!cam_info) {
            LOG_WARN(HLE_DISPATCHER, "Camera info pointer is null, no info will be filled");
        }

        drivers::camera::collection *collection = drivers::camera::get_collection();
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();

        // Creation may sometimes wait for the main thread to ask for permission
        // Unlock the kernel a bit here so we are not pushing deadlock to other workers! (Seems like a hack)
        kernel_system *kern = sys->get_kernel_system();

        kern->unlock();
        std::unique_ptr<drivers::camera::instance> inst = collection->make_camera(static_cast<std::uint32_t>(index));
        kern->lock();

        if (inst == nullptr) {
            LOG_ERROR(HLE_DISPATCHER, "Instantiate new camera access instance failed!");
            return epoc::error_general;
        }

        std::unique_ptr<epoc_camera> native_cam_wrap = std::make_unique<epoc_camera>(inst);
        native_cam_wrap->cached_info_ = native_cam_wrap->impl_->get_info();

        if (cam_info) {
            *cam_info = native_cam_wrap->cached_info_;
        }

        object_manager<epoc_camera>::handle result_h = dispatcher->cameras_.add_object(native_cam_wrap);

        return static_cast<std::int32_t>(result_h);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_claim, std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);
        if (!cam) {
            return epoc::error_bad_handle;
        }
        return cam->impl_->reserve() ? epoc::error_none : epoc::error_in_use;
    }

    BRIDGE_FUNC_DISPATCHER(void, ecam_release, std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);
        if (cam) {
            cam->impl_->release();
        }
    }

    // NOTE: Power on/power off is kind of irrelevant in morden days...
    // Specifically, no API provides these functions. The power can be managed by the driver
    // We are just gonna return true/false here. But still reserve these functions when it's needed.
    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_power_on, std::uint32_t handle) {
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_power_off, std::uint32_t handle) {
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_query_still_image_size, std::uint32_t handle,
                           std::uint32_t format, std::int32_t size_index, eka2l1::vec2 *size) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);
        if (!cam) {
            return epoc::error_bad_handle;
        }
        if (!size) {
            return epoc::error_argument;
        }
        if (size_index < 0) {
            LOG_TRACE(HLE_DISPATCHER, "Negative size index asked in get still image size!");
            return epoc::error_argument;
        }
        if ((cam->cached_info_.supported_image_formats_ & format) == 0) {
            LOG_TRACE(HLE_DISPATCHER, "Camera format is not supported (format={}). Query failed", format);
            return epoc::error_not_supported;
        }
        drivers::camera::frame_format format_casted = static_cast<drivers::camera::frame_format>(format);
        if (cam->cached_frame_sizes_.find(format_casted) == cam->cached_frame_sizes_.end()) {
            cam->cached_frame_sizes_[format_casted] = cam->impl_->supported_output_image_sizes(format_casted);
        }
        if (cam->cached_frame_sizes_[format_casted].size() <= size_index) {
            LOG_TRACE(HLE_DISPATCHER, "Camera size index is out of range ([0..{}], value asked={})",
                      cam->cached_frame_sizes_[format_casted].size(), size_index);
        }
        *size = cam->cached_frame_sizes_[format_casted][static_cast<std::uint32_t>(size_index)];
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_take_image, std::uint32_t handle, std::int32_t size_index,
                           std::int32_t format, eka2l1::rect *clip_rect, eka2l1::ptr<epoc::request_status> status) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);
        kernel_system *kern = sys->get_kernel_system();

        if (!cam) {
            return epoc::error_bad_handle;
        }

        if (size_index < 0) {
            LOG_TRACE(HLE_DISPATCHER, "Negative size index asked in get still image size!");
            return epoc::error_argument;
        }
        if ((cam->cached_info_.supported_image_formats_ & format) == 0) {
            LOG_TRACE(HLE_DISPATCHER, "Camera format is not supported (format={}). Query failed", format);
            return epoc::error_not_supported;
        }

        const std::lock_guard<std::mutex> guard(cam->lock_);

        if (!cam->done_notify_.empty()) {
            LOG_TRACE(HLE_DISPATCHER, "Another camera operation is pending on the same instance!");
            return epoc::error_in_use;
        }

        cam->done_notify_ = epoc::notify_info(status, kern->crr_thread());

        cam->impl_->capture_image(size_index, static_cast<drivers::camera::frame_format>(format),
                                  [cam, kern](const void *buffer, std::size_t buffer_size, int err) {
            const std::lock_guard<std::mutex> guard(cam->lock_);
            kern->lock();

            if (err < 0) {
                cam->done_notify_.complete(epoc::error_general);
            } else {
                // Copy data to temporary buffer
                cam->received_image_frame_[0].resize(buffer_size);
                std::memcpy(cam->received_image_frame_[0].data(), buffer, buffer_size);

                cam->current_frame_index_ = 0;
                cam->done_notify_.complete(epoc::error_none);
            }

            kern->unlock();
        });

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_receive_image, std::uint32_t handle, std::int32_t *size, void *data_ptr) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);

        if (!cam) {
            return epoc::error_bad_handle;
        }

        if ((!size && !data_ptr) || (data_ptr && (*size == 0))) {
            return epoc::error_argument;
        }

        const std::lock_guard<std::mutex> guard(cam->lock_);
        if (cam->current_frame_index_ < 0) {
            if (size) {
                *size = 0;
            }

            return epoc::error_none;
        }

        if (!data_ptr) {
            *size = static_cast<std::int32_t>(cam->received_image_frame_[cam->current_frame_index_].size());
        } else {
            const std::int32_t copy_size = common::min<std::int32_t>(*size,
                 static_cast<std::int32_t>(cam->received_image_frame_[cam->current_frame_index_].size()));

            std::memcpy(data_ptr, cam->received_image_frame_[cam->current_frame_index_].data(), copy_size);

            *size = copy_size;
            cam->current_frame_index_--;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_start_viewfinder, std::uint32_t handle, eka2l1::vec2 *size,
                           std::int32_t display_mode, eka2l1::ptr<epoc::request_status> status) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);
        kernel_system *kern = sys->get_kernel_system();

        if (!cam) {
            return epoc::error_bad_handle;
        }

        if (!size || (size->x <= 0) || (size->y <= 0)) {
            return epoc::error_argument;
        }

        const std::lock_guard<std::mutex> guard(cam->lock_);

        if (!cam->done_notify_.empty()) {
            LOG_TRACE(HLE_DISPATCHER, "Another camera operation is pending on the same instance!");
            return epoc::error_in_use;
        }

        cam->done_notify_ = epoc::notify_info(status, kern->crr_thread());

        drivers::camera::frame_format capture_format;

        switch (static_cast<epoc::display_mode>(display_mode)) {
            case epoc::display_mode::color16m:
            case epoc::display_mode::color16ma:
            case epoc::display_mode::color16map:
            case epoc::display_mode::color16mu:
                capture_format = drivers::camera::FRAME_FORMAT_FBSBMP_COLOR16MU;
                break;

            case epoc::display_mode::color64k:
                capture_format = drivers::camera::FRAME_FORMAT_FBSBMP_COLOR64K;
                break;

            default:
                LOG_TRACE(HLE_DISPATCHER, "Format 0x{:X} is not supported for frame receving!", display_mode);
                return epoc::error_not_supported;
        }

        cam->impl_->receive_viewfinder_feed(*size, capture_format, [cam]() {
            return (cam->current_frame_index_ < static_cast<std::int32_t>(epoc_camera::QUEUE_MAX_PENDING - 1));
        }, [cam, kern](const void *buffer, std::size_t buffer_size, int err) {
            std::unique_lock<std::mutex> guard(cam->lock_);
            if (cam->current_frame_index_ == static_cast<std::int32_t>(epoc_camera::QUEUE_MAX_PENDING - 1)) {
                return;
            }

            guard.unlock();

            kern->lock();
            guard.lock();

            if (err < 0) {
                cam->done_notify_.complete(epoc::error_general);
            } else {
                cam->received_image_frame_[++cam->current_frame_index_].resize(buffer_size);
                std::memcpy(cam->received_image_frame_[cam->current_frame_index_].data(), buffer, buffer_size);

                cam->done_notify_.complete(epoc::error_none);
            }

            kern->unlock();
        });

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_next_viewfinder_frame, std::uint32_t handle, eka2l1::ptr<epoc::request_status> status) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);
        kernel_system *kern = sys->get_kernel_system();

        if (!cam) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(cam->lock_);
        epoc::notify_info done_info(status, kern->crr_thread());

        if (cam->current_frame_index_ >= 0) {
            done_info.complete(epoc::error_none);
        } else {
            cam->done_notify_ = done_info;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_stop_viewfinder_frame, std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);

        if (!cam) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(cam->lock_);
        cam->impl_->stop_viewfinder_feed();

        if (!cam->done_notify_.empty()) {
            cam->done_notify_.complete(epoc::error_cancel);
        }

        cam->current_frame_index_ = -1;
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, ecam_set_parameter, std::uint32_t handle, std::uint32_t key, std::uint32_t value) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        epoc_camera *cam = dispatcher->cameras_.get_object(handle);

        if (!cam) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(cam->lock_);
        if (!cam->impl_->set_parameter(static_cast<drivers::camera::parameter_key>(key), value)) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }
}
