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

#include <common/log.h>
#include <common/cvt.h>
#include <dispatch/video.h>
#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <kernel/kernel.h>
#include <services/window/window.h>
#include <services/window/classes/winuser.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1::dispatch {
    bool posting_target_free_check_func(epoc_video_posting_target &data) {
        return !data.target_window_;
    }

    void posting_target_free_func(epoc_video_posting_target &data) {
        data.target_window_ = nullptr;
    }

    epoc_video_player::epoc_video_player(drivers::graphics_driver *grdrv, drivers::audio_driver *auddrv)
        : image_handle_(0)
        , video_player_(nullptr)
        , driver_(grdrv)
        , custom_stream_(nullptr)
        , rotation_(ROTATION_TYPE_NONE)
        , postings_(posting_target_free_check_func, posting_target_free_func) {
        video_player_ = drivers::new_best_video_player(auddrv);
        video_player_->set_image_frame_available_callback([](void *userdata, const std::uint8_t *data, const std::size_t data_size) {
            epoc_video_player *self = reinterpret_cast<epoc_video_player*>(userdata);
            if (self) {
                self->post_new_image(data, data_size);
            }
        }, this);

        video_player_->set_play_complete_callback([](void *userdata, const int sts) {
            epoc_video_player *self = reinterpret_cast<epoc_video_player*>(userdata);
            if (self) {
                self->on_play_done(sts);
            }
        }, this);
    }

    epoc_video_player::~epoc_video_player() {
        if (image_handle_) {
            drivers::graphics_command_builder builder;
            builder.destroy(image_handle_);

            drivers::command_list list = builder.retrieve_command_list();
            driver_->submit_command_list(list);
        }
    }

    std::int32_t epoc_video_player::register_window(kernel_system *kern, window_server *serv, const std::uint32_t wss_handle, const std::uint32_t win_handle) {
        if (!serv) {
            return -1;
        }

        service::session *ss = kern->get<service::session>(wss_handle);
        if (!ss) {
            LOG_ERROR(HLE_DISPATCHER, "Can't find window session with handle 0x{:X}", wss_handle);
            return -1;
        }

        epoc::window_server_client *real_client = serv->get_client(ss->unique_id());
        if (!real_client) {
            LOG_ERROR(HLE_DISPATCHER, "Unable to find real window session with ID 0x{:X}", ss->unique_id());
            return -1;
        }

        // Get the window
        epoc::canvas_base *the_canvas = reinterpret_cast<epoc::canvas_base*>(real_client->get_object(win_handle));
        if (!the_canvas) {
            LOG_ERROR(HLE_DISPATCHER, "Unable to retrieve the drawable window object! (ID=0x{:X}, WCID=0x{:X})", win_handle, ss->unique_id());
            return -1;
        }

        auto find_res = std::find_if(postings_.begin(), postings_.end(), [the_canvas](const epoc_video_posting_target &target) {
            return target.target_window_ == the_canvas;
        });

        if (find_res != postings_.end()) {
            LOG_ERROR(HLE_DISPATCHER, "Window already registered for video posting! (ID=0x{:X})", the_canvas->id);
            return -1;
        }

        epoc_video_posting_target post_target;
        post_target.target_window_ = the_canvas;

        return static_cast<std::int32_t>(postings_.add(post_target));
    }

    void epoc_video_player::set_target_rect(const std::int32_t managed_handle, const eka2l1::rect &display_rect) {
        if (managed_handle <= 0) {
            return;
        }

        epoc_video_posting_target *target = postings_.get(static_cast<std::size_t>(managed_handle));
        if (target != nullptr) {
            target->display_rect_ = display_rect;
        }
    }

    void epoc_video_player::unregister_window(const std::int32_t managed_handle) {
        if (managed_handle <= 0) {
            return;
        }
        postings_.remove(static_cast<std::size_t>(managed_handle));
    }

    std::uint32_t epoc_video_player::max_volume() const {
        return video_player_->max_volume();
    }

    std::uint32_t epoc_video_player::current_volume() const {
        return video_player_->volume();
    }

    bool epoc_video_player::set_volume(const std::uint32_t vol) {
        return video_player_->set_volume(vol);
    }

    void epoc_video_player::play(const std::uint64_t *range) {
        video_player_->play(range);
    }

    void epoc_video_player::close() {
        video_player_->close();

        if (image_handle_) {
            drivers::graphics_command_builder builder;
            builder.destroy(image_handle_);

            drivers::command_list list = builder.retrieve_command_list();
            driver_->submit_command_list(list);

            image_handle_ = 0;
        }
    }
    
    void epoc_video_player::stop() {
        video_player_->stop();
    }

    bool epoc_video_player::open_file(const std::u16string &real_path) {
        return video_player_->open_file(common::ucs2_to_utf8(real_path));
    }

    bool epoc_video_player::open_with_custom_stream(std::unique_ptr<common::ro_stream> &stream) {
        if (video_player_->open_custom_io(*stream)) {
            custom_stream_ = std::move(stream);
            return true;
        }

        return false;
    }

    void epoc_video_player::set_rotation(const int rotation) {
        rotation_ = common::clamp(ROTATION_TYPE_NONE, ROTATION_TYPE_CLOCKWISE270, static_cast<rotation_type>(rotation));
    }

    void epoc_video_player::post_new_image(const std::uint8_t *buffer_data, const std::size_t buffer_size) {
        const eka2l1::vec2 vid_size = video_player_->get_video_size();
        const eka2l1::vec3 vid_size_v3 = eka2l1::vec3(vid_size.x, vid_size.y, 0);

        for (auto &posting: postings_) {
            if (posting.target_window_ == nullptr) {
                continue;
            }

            const std::lock_guard<std::mutex> guard(posting.target_window_->scr->screen_mutex);

            if (!image_handle_) {
                image_handle_ = drivers::create_texture(driver_, 2, 0, drivers::texture_format::rgba, drivers::texture_format::rgba,
                    drivers::texture_data_type::ubyte, buffer_data, buffer_size, vid_size_v3);
            } else {
                posting.target_window_->driver_builder_.update_texture(image_handle_, reinterpret_cast<const char*>(buffer_data), buffer_size, 0, drivers::texture_format::rgba,
                    drivers::texture_data_type::ubyte, eka2l1::vec3(0, 0, 0), vid_size_v3);
            }

            eka2l1::rect dest_rect = posting.display_rect_;
            dest_rect.top += posting.target_window_->abs_rect.top;
            dest_rect.scale(posting.target_window_->scr->display_scale_factor);

            // Try to change position for good rotation
            switch (rotation_) {
            case 1:
                dest_rect.top.x += dest_rect.size.x;
                break;

            case 2:
                dest_rect.top.x += dest_rect.size.x;
                dest_rect.top.y += dest_rect.size.y;
                break;

            case 3:
                dest_rect.top.y += dest_rect.size.y;
                break;

            default:
                break;
            }

            if (rotation_ & 1) {
                std::swap(dest_rect.size.x, dest_rect.size.y);
            }

            posting.target_window_->driver_builder_.set_texture_filter(image_handle_, false, drivers::filter_option::linear);
            posting.target_window_->driver_builder_.set_texture_filter(image_handle_, true, drivers::filter_option::linear);
            posting.target_window_->driver_builder_.draw_bitmap(image_handle_, 0, dest_rect, eka2l1::rect(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0)), eka2l1::vec2(0, 0), rotation_ * 90.0f);
            posting.target_window_->content_changed(true);

            posting.target_window_->try_update(nullptr);
        }
    }

    std::uint64_t epoc_video_player::position() const {
        return video_player_ ? video_player_->position() : 0;
    }

    bool epoc_video_player::set_done_notify(epoc::notify_info &info) {
        if (!play_done_notify_.empty()) {
            return false;
        }

        play_done_notify_ = info;
        return true;
    }

    void epoc_video_player::cancel_done_notify() {
        play_done_notify_.complete(epoc::error_cancel);
    }
    
    void epoc_video_player::on_play_done(const int error) {
        if (error == 0) {
            play_done_notify_.complete(epoc::error_none);
        } else {
            play_done_notify_.complete(epoc::error_general);
        }
    }
    
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, evideo_player_inst) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::graphics_driver *grdrv = sys->get_graphics_driver();
        drivers::audio_driver *auddrv = sys->get_audio_driver();

        std::unique_ptr<epoc_video_player> player = std::make_unique<epoc_video_player>(grdrv, auddrv);
        return dispatcher->video_player_container_.add_object(player);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_destroy, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        return dispatcher->video_player_container_.remove_object(handle);
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_register_window, const std::uint32_t handle, const std::uint32_t wss_handle, const std::uint32_t win_ws_handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        const std::int32_t result = player->register_window(sys->get_kernel_system(), dispatcher->winserv_, wss_handle, win_ws_handle);

        if (result < 0) {
            return epoc::error_bad_handle;
        }

        return result;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_display_rect, const std::uint32_t handle, const std::int32_t managed_win_handle, const eka2l1::rect *disp_rect) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        eka2l1::rect transformed_rect = *disp_rect;
        transformed_rect.transform_from_symbian_rectangle();

        player->set_target_rect(managed_win_handle, transformed_rect);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_unregister_window, const std::uint32_t handle, const std::int32_t managed_handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        player->unregister_window(managed_handle);
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_max_volume, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(player->max_volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_current_volume, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(player->current_volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_volume, const std::uint32_t handle, const std::int32_t new_vol) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        if (!player->set_volume(static_cast<std::uint32_t>(new_vol))) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_rotation, const std::uint32_t handle, const int rot_type) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        player->set_rotation(rot_type);
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_open_file, const std::uint32_t handle, epoc::desc16 *filename) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        kernel_system *kern = sys->get_kernel_system();                
        kernel::process *crr_pr = kern->crr_process();

        const std::u16string virtual_filename = filename->to_std_string(crr_pr);
        LOG_TRACE(HLE_DISPATCHER, "Open a new video to play with path: {}", common::ucs2_to_utf8(virtual_filename));

        io_system *ios = sys->get_io_system();

        if (!ios->exist(virtual_filename)) {
            return epoc::error_not_found;            
        }

        std::optional<std::u16string> real_filename = ios->get_raw_path(virtual_filename);

        bool open_res = false;

        if (real_filename.has_value()) {
            open_res = player->open_file(real_filename.value());            
        } else {
            symfile f = ios->open_file(virtual_filename, READ_MODE | BIN_MODE);
            if (!f) {
                return epoc::error_not_found;
            }

            std::unique_ptr<common::ro_stream> cs_stream = std::make_unique<eka2l1::ro_file_stream>(f.get());
            open_res = player->open_with_custom_stream(cs_stream);
        }

        if (!open_res) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_play, const std::uint32_t handle, const std::uint64_t *range) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        player->play(range);
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_done_notify, const std::uint32_t handle, eka2l1::ptr<epoc::request_status> sts) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        epoc::notify_info new_notify(sts, sys->get_kernel_system()->crr_thread());

        if (!player->set_done_notify(new_notify)) {
            LOG_ERROR(DRIVER_VID, "There's already a pending done notification!");
            return epoc::error_in_use;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_cancel_done_notify, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        player->cancel_done_notify();
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_close, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        player->close();
        return epoc::error_none;
    }

    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_stop, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        player->stop();
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_position, const std::uint32_t handle, std::uint64_t *position_us) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        *position_us = player->position();
        return epoc::error_none;
    }
}