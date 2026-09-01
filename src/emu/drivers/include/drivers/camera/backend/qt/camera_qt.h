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
    class instance_qt;

    // Qt Multimedia objects of one camera, plus the callbacks the guest armed.
    // Defined in the implementation so no Qt header leaks out. It outlives
    // instance_qt: the camera thread, not a guest thread, tears it down.
    struct session_qt;

    session_qt *make_session_qt(const std::uint32_t camera_index);
    void destroy_session_qt(session_qt *session);

    struct session_qt_deleter {
        void operator()(session_qt *session) const {
            destroy_session_qt(session);
        }
    };

    // Every host video input, the host's own default first.
    class collection_qt : public collection {
    private:
        friend class instance_qt;

        std::map<int, instance_qt *> current_reserved_;
        std::mutex reserve_lock_;

    public:
        std::uint32_t count() const override;
        std::unique_ptr<instance> make_camera(const std::uint32_t camera_index) override;
    };

    class instance_qt : public instance {
    private:
        friend class collection_qt;

        collection_qt *collection_;

        int index_;
        std::unique_ptr<session_qt, session_qt_deleter> session_;

        std::uint32_t stub_optical_zoom_;
        std::uint32_t stub_exposure_;
        std::uint32_t stub_digital_zoom_;
        std::uint32_t stub_contrast_;
        std::uint32_t stub_brightness_;
        std::uint32_t stub_white_balance_;
        std::uint32_t flash_mode_;

    public:
        explicit instance_qt(collection_qt *collection, const int index, session_qt *session);
        ~instance_qt() override;

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
    };
}
