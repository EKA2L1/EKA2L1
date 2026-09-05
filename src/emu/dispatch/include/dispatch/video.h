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

#include <common/vecx.h>
#include <common/container.h>
#include <dispatch/def.h>
#include <drivers/video/video.h>
#include <drivers/itc.h>
#include <mem/ptr.h>
#include <services/window/classes/winuser.h>
#include <utils/des.h>
#include <utils/reqsts.h>


namespace eka2l1 {
    namespace drivers {
        class audio_driver;
    }

    class window_server;
    class kernel_system;
}

namespace eka2l1::dispatch {
    struct epoc_video_posting_target {
        epoc::canvas_base *target_window_ = nullptr;
        epoc::surface_configuration config_;
        std::weak_ptr<epoc::window_surface> displaced_surface_;
        epoc::surface_configuration displaced_config_;
    };

    struct video_window_geometry {
        eka2l1::rect extent;
        eka2l1::rect clip;
    };

    class epoc_video_player : public epoc::canvas_observer {
    private:
        std::shared_ptr<epoc::window_surface> surface_;
        bool surface_created_ = false;
        bool legacy_display_;
        std::optional<eka2l1::rect> crop_region_;
        drivers::video_player_instance video_player_;

        common::identity_container<epoc_video_posting_target> postings_;

        std::unique_ptr<common::ro_stream> custom_stream_;
        epoc::notify_info play_done_notify_;

        enum rotation_type : int {
            ROTATION_TYPE_NONE,
            ROTATION_TYPE_CLOCKWISE90,
            ROTATION_TYPE_CLOCKWISE180,
            ROTATION_TYPE_CLOCKWISE270
        };
        
        rotation_type rotation_;

        kernel_system *kern_;
        void attach_target(epoc_video_posting_target &target);
        void detach_target(epoc_video_posting_target &target);

    public:
        explicit epoc_video_player(kernel_system *kern, drivers::graphics_driver *grdrv, drivers::audio_driver *auddrv, bool legacy_display = true);
        ~epoc_video_player();

        std::int32_t register_window(kernel_system *kern, window_server *serv, const std::uint32_t wss_handle, const std::uint32_t win_handle);
        void set_target_rect(const std::int32_t managed_handle, const eka2l1::rect &display_rect);
        std::int32_t set_target_geometry(std::int32_t managed_handle, const video_window_geometry &geometry);
        std::int32_t set_crop_region(const eka2l1::rect &crop);
        void unregister_window(const std::int32_t managed_handle);

        std::uint32_t max_volume() const;
        std::uint32_t current_volume() const;
        bool set_volume(const std::uint32_t vol);

        bool open_file(const std::u16string &real_path);
        bool open_with_custom_stream(std::unique_ptr<common::ro_stream> &stream);

        void set_rotation(const int rotation);

        void play(const std::uint64_t *range);
        void close();
        void stop();

        bool set_done_notify(epoc::notify_info &info);
        void cancel_done_notify();
        void on_play_done(const int sts);

        void post_new_image(const std::uint8_t *buffer_data, const std::size_t buffer_size);
        std::uint64_t position() const;

        void on_window_size_changed(epoc::canvas_interface *obj) override;
        void on_window_destroyed(epoc::canvas_interface *obj) override;
    };

    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, evideo_player_inst);
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, evideo_player_inst_with_version, const std::uint32_t version);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_crop, const std::uint32_t handle, const eka2l1::rect *crop);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_destroy, const std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_register_window, const std::uint32_t handle, const std::uint32_t wss_handle, const std::uint32_t win_ws_handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_display_rect, const std::uint32_t handle, const std::int32_t managed_handle, const eka2l1::rect *disp_rect);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_geometry, const std::uint32_t handle, const std::int32_t managed_handle, const video_window_geometry *geometry);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_unregister_window, const std::uint32_t handle, const std::int32_t managed_handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_open_file, const std::uint32_t handle, epoc::desc16 *filename);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_max_volume, const std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_current_volume, const std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_volume, const std::uint32_t handle, const std::int32_t new_vol);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_rotation, const std::uint32_t handle, const int rot_type);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_play, const std::uint32_t handle, const std::uint64_t *range);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_stop, const std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_close, const std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_done_notify, const std::uint32_t handle, eka2l1::ptr<epoc::request_status> sts);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_cancel_done_notify, const std::uint32_t handle);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_position, const std::uint32_t handle, std::uint64_t *position_us);
}
