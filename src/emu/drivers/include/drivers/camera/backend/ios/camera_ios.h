/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <drivers/camera/camera.h>
#include <drivers/camera/camera_collection.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace eka2l1::drivers::camera {
    class instance_ios;

    // AVFoundation-backed camera collection. Index mapping mirrors the Android
    // backend: 0 = back camera, 1 = front camera, and only one of each is
    // exposed for best guest compatibility.
    class collection_ios : public collection {
    private:
        friend class instance_ios;

        std::map<int, instance_ios *> current_reserved_;
        std::mutex reserve_lock_;

    public:
        std::uint32_t count() const override;
        std::unique_ptr<instance> make_camera(const std::uint32_t camera_index) override;
    };

    class instance_ios : public instance {
    private:
        friend class collection_ios;

        collection_ios *collection_;

        int index_;
        void *device_holder_;       // EKACameraDevice*, bridge-retained

        camera_capture_image_done_callback active_capture_img_callback_;
        camera_capture_image_done_callback active_frame_viewfinder_callback_;
        camera_wants_new_frame_callback wants_new_frame_callback_;

        std::mutex callback_lock_;

        std::uint32_t stub_optical_zoom_;
        std::uint32_t stub_exposure_;
        std::uint32_t stub_digital_zoom_;
        std::uint32_t stub_contrast_;
        std::uint32_t stub_brightness_;
        std::uint32_t stub_white_balance_;
        std::uint32_t flash_mode_;

        void stop_viewfinder_feed_impl(const bool log_if_inactive);

    public:
        explicit instance_ios(collection_ios *collection, const int index, void *device_holder);
        ~instance_ios() override;

        bool set_parameter(const parameter_key key, const std::uint32_t value) override;
        bool get_parameter(const parameter_key key, std::uint32_t &value) override;

        std::vector<frame_format> supported_frame_formats() override;
        std::vector<eka2l1::vec2> supported_output_image_sizes(const frame_format frame_format) override;

        void capture_image(const std::uint32_t resolution_index, const frame_format format,
                           camera_capture_image_done_callback callback) override;

        void receive_viewfinder_feed(const eka2l1::vec2 &size, const frame_format format,
                                     camera_wants_new_frame_callback new_frame_needed_callback,
                                     camera_capture_image_done_callback new_frame_come_callback) override;

        void stop_viewfinder_feed() override;

        bool reserve() override;
        void release() override;

        info get_info() override;

        // Entry points for the AVFoundation delegate queues. Callbacks are
        // copied under callback_lock_ and invoked outside it: the guest-side
        // completion takes the kernel lock, and a guest thread holding the
        // kernel lock may concurrently call stop_viewfinder_feed() — invoking
        // under callback_lock_ would deadlock (same ABBA shape the iOS sensor
        // backend avoids).
        void deliver_viewfinder_frame(const void *bytes, const std::size_t size, const int error);
        void deliver_captured_image(const void *bytes, const std::size_t size, const int error);
        bool wants_new_frame();
    };
}
