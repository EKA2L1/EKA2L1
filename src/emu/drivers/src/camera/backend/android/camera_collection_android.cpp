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

#include <drivers/camera/backend/android/camera_collection_android.h>
#include <drivers/camera/backend/android/camera_android.h>
#include "emulator_camera_jni.h"

#include <common/log.h>

namespace eka2l1::drivers::camera {
    std::uint32_t collection_android::count() const {
        return android::emulator_camera_count();
    }

    std::unique_ptr<instance> collection_android::make_camera(const std::uint32_t camera_index) {
        return std::make_unique<instance_android>(this, static_cast<int>(camera_index));
    }

    void collection_android::handle_image_capture_delivered(int index, const void *bytes, const std::size_t size, const int error) {
        auto ite = current_reserved_.find(index);
        if (ite != current_reserved_.end()) {
            const std::lock_guard<std::mutex> guard(ite->second->callback_lock_);
            if (ite->second->active_capture_img_callback_) {
                ite->second->active_capture_img_callback_(bytes, size, error);
            }
            if (ite->second->should_dispose_callback_after_done_) {
                ite->second->active_capture_img_callback_ = nullptr;
            }
        } else {
            LOG_TRACE(DRIVER_CAM, "Unable to find active reserved camera with index {} for returning capture bytes", index);
        }
    }

    bool collection_android::reserved_wants_new_frame(int index) {
        auto ite = current_reserved_.find(index);
        if (ite != current_reserved_.end()) {
            const std::lock_guard<std::mutex> guard(ite->second->callback_lock_);
            if (ite->second->wants_new_frame_callback_) {
                return ite->second->wants_new_frame_callback_();
            }
        }
        return false;
    }
}