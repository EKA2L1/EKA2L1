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

    epoc_video_player::epoc_video_player(kernel_system *kern, drivers::graphics_driver *, drivers::audio_driver *auddrv, bool legacy_display)
        : surface_(std::make_shared<epoc::window_surface>())
        , legacy_display_(legacy_display)
        , video_player_(nullptr)
        , custom_stream_(nullptr)
        , rotation_(ROTATION_TYPE_NONE)
        , kern_(kern)
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
        close();
        for (auto &posting : postings_) {
            if (posting.target_window_) {
                posting.target_window_->remove_canvas_observer(this);
            }
        }
    }

    void epoc_video_player::attach_target(epoc_video_posting_target &target) {
        if (surface_created_ && target.target_window_) {
            auto *window = target.target_window_;
            const std::lock_guard<std::mutex> guard(window->scr->screen_mutex);
            if (legacy_display_ && window->scr->is_screenplay_architecture()) {
                target.displaced_surface_ = window->background_surface_.surface;
                target.displaced_config_ = window->background_surface_.config;
            }
            window->attach_surface(surface_, target.config_, !window->scr->is_screenplay_architecture());
        }
    }

    void epoc_video_player::detach_target(epoc_video_posting_target &target) {
        auto *window = target.target_window_;
        if (window) {
            const std::lock_guard<std::mutex> guard(window->scr->screen_mutex);
            // The legacy HLE controller borrows the display; WServ itself never restores attachments.
            auto displaced = target.displaced_surface_.lock();
            if (legacy_display_ && displaced && window->background_surface_.surface == surface_) {
                window->attach_surface(displaced, target.displaced_config_);
            } else {
                window->detach_surface(surface_);
            }
        }
        target.displaced_surface_.reset();
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

        auto *object = real_client->get_object(win_handle);
        epoc::canvas_base *the_canvas = dynamic_cast<epoc::canvas_base*>(object);
        if (!the_canvas || the_canvas->win_type == epoc::window_type::backed_up) {
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

        post_target.config_.rotation = static_cast<int>(rotation_) * 90;
        post_target.config_.viewport = crop_region_;
        the_canvas->add_canvas_observer(this);
        attach_target(post_target);
        return static_cast<std::int32_t>(postings_.add(post_target));
    }

    void epoc_video_player::set_target_rect(const std::int32_t managed_handle, const eka2l1::rect &display_rect) {
        set_target_geometry(managed_handle, video_window_geometry{ display_rect, display_rect });
    }

    std::int32_t epoc_video_player::set_target_geometry(std::int32_t managed_handle, const video_window_geometry &geometry) {
        auto *target = managed_handle > 0 ? postings_.get(static_cast<std::size_t>(managed_handle)) : nullptr;
        if (!target || !target->target_window_) {
            return epoc::error_bad_handle;
        }
        if (geometry.extent.size.x < 0 || geometry.extent.size.y < 0 || geometry.clip.size.x < 0 || geometry.clip.size.y < 0) {
            return epoc::error_argument;
        }
        target->config_.extent = geometry.extent;
        target->config_.clip = geometry.clip;
        auto *window = target->target_window_;
        const std::lock_guard<std::mutex> guard(window->scr->screen_mutex);
        window->configure_surface(surface_, target->config_);
        return epoc::error_none;
    }

    std::int32_t epoc_video_player::set_crop_region(const eka2l1::rect &crop) {
        if (crop.size.x < 0 || crop.size.y < 0 || crop.top.x < 0 || crop.top.y < 0) {
            return epoc::error_argument;
        }
        crop_region_ = crop.empty() ? std::nullopt : std::optional<eka2l1::rect>(crop);
        for (auto &posting : postings_) {
            posting.config_.viewport = crop_region_;
            if (posting.target_window_) {
                const std::lock_guard<std::mutex> guard(posting.target_window_->scr->screen_mutex);
                posting.target_window_->configure_surface(surface_, posting.config_);
            }
        }
        return epoc::error_none;
    }

    void epoc_video_player::unregister_window(const std::int32_t managed_handle) {
        if (managed_handle <= 0) {
            return;
        }
        auto *target = postings_.get(static_cast<std::size_t>(managed_handle));
        if (target && target->target_window_) {
            detach_target(*target);
            auto *window = target->target_window_;
            const std::lock_guard<std::mutex> guard(window->scr->screen_mutex);
            window->remove_canvas_observer(this);
        }
        postings_.remove(static_cast<std::size_t>(managed_handle));
    }

    void epoc_video_player::on_window_size_changed(epoc::canvas_interface *obj) {
    }

    void epoc_video_player::on_window_destroyed(epoc::canvas_interface *obj) {
        // Window observers are kernel-serialized; decoder callbacks only use surface_.
        for (auto &posting : postings_) {
            if (posting.target_window_ == obj) {
                posting.target_window_ = nullptr;
            }
        }
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
        if (!surface_) {
            surface_ = std::make_shared<epoc::window_surface>();
        }
        surface_->set_streaming(true);
        if (!surface_created_) {
            surface_created_ = true;
            for (auto &posting : postings_) {
                attach_target(posting);
            }
        } else {
            for (auto &posting : postings_) {
                if (posting.target_window_) {
                    const std::lock_guard<std::mutex> guard(posting.target_window_->scr->screen_mutex);
                    posting.target_window_->surface_damage();
                }
            }
        }
        video_player_->play(range);
    }

    void epoc_video_player::close() {
        video_player_->close();
        for (auto &posting : postings_) {
            detach_target(posting);
        }
        surface_created_ = false;
        surface_.reset();
        custom_stream_.reset();
    }

    void epoc_video_player::stop() {
        video_player_->stop();
        if (surface_) {
            surface_->set_streaming(false);
        }
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
        for (auto &posting : postings_) {
            if (posting.target_window_) {
                posting.config_.rotation = static_cast<int>(rotation_) * 90;
                const std::lock_guard<std::mutex> guard(posting.target_window_->scr->screen_mutex);
                posting.target_window_->configure_surface(surface_, posting.config_);
            }
        }
    }

    void epoc_video_player::post_new_image(const std::uint8_t *buffer_data, const std::size_t buffer_size) {
        surface_->publish_pixels(buffer_data, buffer_size, video_player_->get_video_size());
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
        surface_->set_streaming(false);
        // Fired on the decode thread. Completing a guest notify needs the kernel
        // lock, and the requester may already be gone. The bridge calls that join
        // this thread release the kernel lock around the join, so taking it here
        // cannot deadlock.
        kern_->lock();

        if (!play_done_notify_.empty() && kern_->is_thread_alive(play_done_notify_.requester)) {
            play_done_notify_.complete((error == 0) ? epoc::error_none : epoc::error_general);
        } else {
            play_done_notify_.sts = 0;
        }

        kern_->unlock();
    }
    
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, evideo_player_inst) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::graphics_driver *grdrv = sys->get_graphics_driver();
        drivers::audio_driver *auddrv = sys->get_audio_driver();

        std::unique_ptr<epoc_video_player> player = std::make_unique<epoc_video_player>(sys->get_kernel_system(), grdrv, auddrv);
        return dispatcher->video_player_container_.add_object(player);
    }

    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, evideo_player_inst_with_version, const std::uint32_t version) {
        if (version != 1 && version != 2) {
            return eka2l1::ptr<void>(0);
        }
        auto player = std::make_unique<epoc_video_player>(sys->get_kernel_system(), sys->get_graphics_driver(),
            sys->get_audio_driver(), version == 1);
        return sys->get_dispatcher()->video_player_container_.add_object(player);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_crop, const std::uint32_t handle, const eka2l1::rect *crop) {
        auto *player = sys->get_dispatcher()->video_player_container_.get_object(handle);
        if (!player) {
            return epoc::error_bad_handle;
        }
        if (!crop) {
            return epoc::error_argument;
        }
        auto transformed = *crop;
        transformed.transform_from_symbian_rectangle();
        return player->set_crop_region(transformed);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_destroy, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        // Decoder completion takes the kernel lock, so join before removing the player.
        if (player) {
            kernel_system *kern = sys->get_kernel_system();

            kern->unlock();
            player->stop();
            kern->lock();
        }

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

        if (!disp_rect) {
            return epoc::error_argument;
        }
        eka2l1::rect transformed_rect = *disp_rect;
        transformed_rect.transform_from_symbian_rectangle();

        player->set_target_rect(managed_win_handle, transformed_rect);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_set_geometry, const std::uint32_t handle, const std::int32_t managed_handle, const video_window_geometry *geometry) {
        auto *player = sys->get_dispatcher()->video_player_container_.get_object(handle);
        if (!player) {
            return epoc::error_bad_handle;
        }
        if (!geometry) {
            return epoc::error_argument;
        }
        video_window_geometry transformed = *geometry;
        transformed.extent.transform_from_symbian_rectangle();
        transformed.clip.transform_from_symbian_rectangle();
        return player->set_target_geometry(managed_handle, transformed);
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

        // Stop outside the kernel lock; attachment changes in play need it held.
        kernel_system *kern = sys->get_kernel_system();

        kern->unlock();
        player->stop();
        kern->lock();
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

        kernel_system *kern = sys->get_kernel_system();

        kern->unlock();
        player->stop();
        kern->lock();
        player->close();

        return epoc::error_none;
    }

    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, evideo_player_stop, const std::uint32_t handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::epoc_video_player *player = dispatcher->video_player_container_.get_object(handle);

        if (!player) {
            return epoc::error_bad_handle;
        }

        kernel_system *kern = sys->get_kernel_system();

        kern->unlock();
        player->stop();
        kern->lock();

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
