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

#include <drivers/camera/backend/android/camera_android.h>
#include <drivers/camera/backend/android/camera_collection_android.h>
#include "emulator_camera_jni.h"

#include <common/log.h>

namespace eka2l1::drivers::camera {
#define CHECK_IF_RESERVED(action_on_fail) if (managed_handle_ < 0) { LOG_ERROR(DRIVER_CAM, "Camera instance is not reserved!"); action_on_fail; }

    instance_android::instance_android(collection_android *collection, const int index)
        : collection_(collection)
        , index_(index)
        , managed_handle_(-1)
        , active_capture_img_callback_(nullptr)
        , wants_new_frame_callback_(nullptr)
        , should_dispose_callback_after_done_(false)
        , stub_exposure_(EXPOSURE_MODE_AUTO)
        , stub_digital_zoom_(1) {
        managed_handle_ = android::emulator_camera_initialize(index_);
        if (managed_handle_ < 0) {
            LOG_ERROR(DRIVER_CAM, "Unable to grab camera managed instance!");
        }
    }

    instance_android::~instance_android() {
        release();
    }

    bool instance_android::set_parameter(const parameter_key key, const std::uint32_t value) {
        CHECK_IF_RESERVED(return false);

        switch (key) {
            case PARAMETER_KEY_FLASH:
                return android::emulator_camera_set_flash_mode(managed_handle_, static_cast<int>(value));

            case PARAMETER_KEY_EXPOSURE:
                stub_exposure_ = value;
                return true;

            case PARAMETER_KEY_DIGITAL_ZOOM:
                stub_digital_zoom_ = value;
                return true;

            default:
                LOG_WARN(DRIVER_CAM, "Unsupported parameter key {} to set value", static_cast<int>(key));
                break;
        }

        return false;
    }

    bool instance_android::get_parameter(const parameter_key key, std::uint32_t &value) {
        CHECK_IF_RESERVED(return false);

        switch (key) {
            case PARAMETER_KEY_FLASH:
                value = static_cast<std::uint32_t>(android::emulator_camera_get_flash_mode(managed_handle_));
                break;

            case PARAMETER_KEY_EXPOSURE:
                value = stub_exposure_;
                break;

            case PARAMETER_KEY_DIGITAL_ZOOM:
                value = stub_digital_zoom_;
                break;

            default:
                LOG_WARN(DRIVER_CAM, "Unsupported parameter key {} to get value", static_cast<int>(key));
                return false;
        }

        return true;
    }

    std::vector<frame_format> instance_android::supported_frame_formats() {
        std::vector<int> frame_format_ints = android::emulator_camera_get_supported_image_output_formats();
        std::vector<frame_format> results(frame_format_ints.size());

        for (std::size_t i = 0; i < frame_format_ints.size(); i++) {
            results[i] = static_cast<frame_format>(frame_format_ints[i]);
        }

        return results;
    }

    std::vector<eka2l1::vec2> instance_android::supported_output_image_sizes(const frame_format frame_format) {
        return android::emulator_camera_get_output_image_sizes(managed_handle_);
    }

    bool instance_android::reserve() {
        if (collection_->current_reserved_[index_] != nullptr) {
            LOG_ERROR(DRIVER_CAM, "Another camera instance is currently reserved the camera for operations!");
            return false;
        }

        collection_->current_reserved_[index_] = this;
        return true;
    }

    void instance_android::release() {
        if (collection_->current_reserved_[index_] == this) {
            collection_->current_reserved_[index_] = nullptr;
        }
    }

    info instance_android::get_info() {
        info result;
        result.camera_direction_ = android::emulator_camera_is_facing_front(managed_handle_) ?
                DIRECTION_FRONT : DIRECTION_BACK;
        result.num_image_sizes_supported_ = static_cast<std::int32_t>(
                android::emulator_camera_get_output_image_sizes(managed_handle_).size());
        result.flash_modes_supported_ = FLASH_MODE_OFF | FLASH_MODE_AUTO | FLASH_MODE_FORCED | FLASH_MODE_VIDEO_LIGHT;
        result.options_supported_ = CAPTURE_OPTION_ALL;
        result.supported_image_formats_ = 0;

        std::vector<int> supported_frame_formats = android::emulator_camera_get_supported_image_output_formats();
        for (std::size_t i = 0; i < supported_frame_formats.size(); i++) {
            result.supported_image_formats_ |= *reinterpret_cast<std::uint32_t*>(&supported_frame_formats[i]);
        }

        return result;
    }

    void instance_android::receive_viewfinder_feed(const eka2l1::vec2 &size, const frame_format format,
         camera_wants_new_frame_callback new_frame_needed_callback,
         camera_capture_image_done_callback new_frame_come_callback) {
        if (collection_->current_reserved_[index_] != this) {
            LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to receive viewfinder feed!");
            return;
        }

        if (active_capture_img_callback_) {
            LOG_ERROR(DRIVER_CAM, "Another operation is active on this camera!");
            return;
        }

        if (!new_frame_come_callback || !new_frame_needed_callback) {
            LOG_ERROR(DRIVER_CAM, "One of the viewfinder receive callback are null. The operation is skipped!");
            return;
        }

        const std::lock_guard<std::mutex> guard(callback_lock_);
        active_capture_img_callback_ = new_frame_come_callback;
        wants_new_frame_callback_ = new_frame_needed_callback;
        should_dispose_callback_after_done_ = false;

        if (!android::emulator_camera_receive_viewfinder_feed(managed_handle_, size.x, size.y,
            static_cast<int>(format))) {
            LOG_ERROR(DRIVER_CAM, "Unable to request image capture!");
            active_capture_img_callback_(nullptr, 0, -1);
            active_capture_img_callback_ = nullptr;

            return;
        }
    }

    void instance_android::stop_viewfinder_feed() {
        if (collection_->current_reserved_[index_] != this) {
            LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to stop viewfinder feed!");
            return;
        }

        const std::lock_guard<std::mutex> guard(callback_lock_);

        android::emulator_camera_stop_viewfinder_feed(managed_handle_);
        active_capture_img_callback_ = nullptr;
    }

    void instance_android::capture_image(const std::uint32_t resolution_index, const frame_format format,
                       camera_capture_image_done_callback callback) {
        if (!callback) {
            LOG_ERROR(DRIVER_CAM, "No capture image callback provided. Skipping image capture!");
            return;
        }

        if (collection_->current_reserved_[index_] != this) {
            LOG_ERROR(DRIVER_CAM, "Camera is not yet reserved to capture image!");
            return;
        }

        if (active_capture_img_callback_) {
            LOG_ERROR(DRIVER_CAM, "Another operation is active on this camera!");
            return;
        }

        const std::lock_guard<std::mutex> guard(callback_lock_);

        active_capture_img_callback_ = callback;
        should_dispose_callback_after_done_ = true;

        if (!android::emulator_camera_capture_image(managed_handle_, static_cast<int>(resolution_index),
                                                    static_cast<int>(format))) {
            LOG_ERROR(DRIVER_CAM, "Unable to request image capture!");
            callback(nullptr, 0, -1);

            active_capture_img_callback_ = nullptr;

            return;
        }
    }
}