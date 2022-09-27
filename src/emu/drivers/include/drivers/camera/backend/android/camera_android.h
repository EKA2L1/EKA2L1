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

#pragma once

#include <drivers/camera/camera.h>
#include <mutex>

namespace eka2l1::drivers::camera {
    class collection_android;

    class instance_android : public instance {
    private:
        friend class collection_android;

        collection_android *collection_;

        int index_;
        int managed_handle_;

        camera_capture_image_done_callback active_capture_img_callback_;
        camera_wants_new_frame_callback wants_new_frame_callback_;

        bool should_dispose_callback_after_done_;
        std::mutex callback_lock_;

    public:
        explicit instance_android(collection_android *collection, const int index);
        ~instance_android() override;

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

        const int managed_handle() const {
            return managed_handle_;
        }

        const int device_index() const {
            return index_;
        }
    };
}