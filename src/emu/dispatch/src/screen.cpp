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

#include <common/log.h>
#include <config/config.h>
#include <dispatch/dispatcher.h>
#include <dispatch/screen.h>

#include <drivers/graphics/graphics.h>
#include <kernel/kernel.h>
#include <services/window/common.h>
#include <services/window/window.h>
#include <services/window/classes/wingroup.h>
#include <system/epoc.h>

#include <fstream>

namespace eka2l1::dispatch {    
    void screen_post_transferer::construct(ntimer *timing) {
        vsync_notify_event_ = timing->register_event("VSyncNotifyEvent", [this](const std::uint64_t data, const int cycles_late) {
            epoc::notify_info *info = reinterpret_cast<epoc::notify_info*>(data);
            this->complete_notify(info);
        });

        timing_ = timing;
    }

    void screen_post_transferer::complete_notify(epoc::notify_info *info) {
        const std::lock_guard<std::mutex> guard(lock_);

        auto ite = std::find(vsync_notifies_.begin(), vsync_notifies_.end(), info);
        if (ite != vsync_notifies_.end()) {
            vsync_notifies_.erase(ite);
        }

        info->complete(epoc::error_none);
        delete info;
    }

    void screen_post_transferer::wait_vsync(epoc::screen *scr, epoc::notify_info &info) {
        std::uint64_t next_sync = 0;
        scr->vsync(timing_, next_sync);

        if (next_sync == 0) {
            info.complete(epoc::error_none);
            return;
        }

        const std::lock_guard<std::mutex> guard(lock_);

        epoc::notify_info *info_copy = new epoc::notify_info;
        *info_copy = info;

        vsync_notifies_.push_back(info_copy);
        timing_->schedule_event(static_cast<std::int64_t>(next_sync), vsync_notify_event_,
            reinterpret_cast<std::uint64_t>(info_copy));
    }

    void screen_post_transferer::cancel_wait_vsync(const epoc::notify_info &info) {
        const std::lock_guard<std::mutex> guard(lock_);
        auto ite = std::find_if(vsync_notifies_.begin(), vsync_notifies_.end(), [=](epoc::notify_info *info_stored) {
            return (info_stored->requester == info.requester) && (info_stored->sts == info.sts);
        });

        if (ite != vsync_notifies_.end()) {
            timing_->unschedule_event(vsync_notify_event_, reinterpret_cast<std::uint64_t>(*ite));
            (*ite)->complete(epoc::error_cancel);

            vsync_notifies_.erase(ite);
        }
    }

    void screen_post_transferer::destroy(drivers::graphics_driver *drv) {
        for (std::size_t i = 0; i < vsync_notifies_.size(); i++) {
            timing_->unschedule_event(vsync_notify_event_, reinterpret_cast<std::uint64_t>(vsync_notifies_[i]));
            delete vsync_notifies_[i];
        }

        vsync_notifies_.clear();
        timing_->remove_event(vsync_notify_event_);

        if (drv) {
            drivers::graphics_command_builder builder;
            
            bool need_send_destroy = false;

            for (std::size_t i = 0; i < infos_.size(); i++) {
                if (infos_[i].transfer_texture_ != 0) {
                    need_send_destroy = true;
                    builder.destroy(infos_[i].transfer_texture_);
                }
            }

            if (need_send_destroy) {
                drivers::command_list retrieved = builder.retrieve_command_list();
                drv->submit_command_list(retrieved);
            }
        }
    }

    drivers::handle screen_post_transferer::transfer_data_to_texture(drivers::graphics_driver *drv, drivers::graphics_command_builder &builder,
        std::int32_t screen_index, std::uint8_t *data, eka2l1::vec2 size, std::int32_t format) {
        if ((screen_index < 0) || (screen_index > 10) || !drv) {
            return 0;
        }

        if (screen_index >= infos_.size()) {
            const std::size_t current_size = infos_.size();
            infos_.resize(screen_index + 1);

            for (std::size_t i = current_size; i < infos_.size(); i++) {
                infos_[i].transfer_texture_ = 0;
            }
        }

        drivers::texture_format data_format = drivers::texture_format::rgba;
        drivers::texture_format internal_format = drivers::texture_format::rgba;
        drivers::texture_data_type type = drivers::texture_data_type::ubyte;

        std::size_t line_stride = 0;

        switch (format) {
        case FORMAT_RGB16_565_LE:
            data_format = drivers::texture_format::rgb;
            internal_format = drivers::texture_format::rgb;
            type = drivers::texture_data_type::ushort_5_6_5;
            line_stride = ((size.x * 2) + 1) / 4 * 4; 

            break;

        case FORMAT_RGB24_888_LE:
            data_format = drivers::texture_format::rgb;
            internal_format = drivers::texture_format::rgb;
            type = drivers::texture_data_type::ubyte;
            line_stride = ((size.x * 3) + 1) / 4 * 4; 

            break;

        case FORMAT_RGB32_X888_LE:
            data_format = drivers::texture_format::rgba;
            internal_format = drivers::texture_format::rgba;
            type = drivers::texture_data_type::ubyte;
            line_stride = (size.x * 4);

            break;

        default:
            LOG_ERROR(HLE_DISPATCHER, "Unhandled format 0x{:X}!", format);
            return 0;
        }

        screen_post_source_info &info = infos_[screen_index];
        if ((info.transfer_texture_ == 0) || (info.transfer_texture_size_.x > size.x) || (info.transfer_texture_size_.y > size.y)
            || (info.format_ != format)) {
            if (info.transfer_texture_ != 0) {
                builder.destroy(info.transfer_texture_);
            }

            info.transfer_texture_ = drivers::create_texture(drv, 2, 0, internal_format, data_format, type, nullptr, 0,
                eka2l1::vec3(size.x, size.y, 0));

            info.transfer_texture_size_ = size;
            info.format_ = format;

            builder.set_texture_filter(info.transfer_texture_, false, drivers::filter_option::linear);
        }

        builder.update_texture(info.transfer_texture_, reinterpret_cast<const char*>(data), line_stride * size.y, 0, data_format, type, eka2l1::vec3(0, 0, 0),
            eka2l1::vec3(size.x, size.y, 0), 0);

        if (format == FORMAT_RGB32_X888_LE) {
            builder.set_swizzle(info.transfer_texture_, drivers::channel_swizzle::blue, drivers::channel_swizzle::green,
                drivers::channel_swizzle::red, drivers::channel_swizzle::one);
        }

        return info.transfer_texture_;
    }

    BRIDGE_FUNC_DISPATCHER(void, update_screen, const std::uint32_t screen_number, const std::uint32_t num_rects, const eka2l1::rect *rect_list) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::graphics_driver *driver = sys->get_graphics_driver();
        kernel_system *kern = sys->get_kernel_system();

        // TODO: Update only some regions specified. Rotation makes it complicated
        epoc::screen *scr = dispatcher->winserv_->get_screens();

        while (scr != nullptr) {
            if (scr->number == screen_number) {
                // Update the DSA screen texture
                const epoc::config::screen_mode &mode_info = scr->current_mode();
                const eka2l1::vec2 screen_size = mode_info.size;

                const char *data_ptr = reinterpret_cast<const char *>(scr->screen_buffer_ptr());
                const std::size_t buffer_size = mode_info.size.x * mode_info.size.y * 4;

                std::uint64_t next_vsync_us = 0;
                scr->vsync(sys->get_ntimer(), next_vsync_us);

                if (next_vsync_us) {
                    kern->crr_thread()->sleep(static_cast<std::uint32_t>(next_vsync_us));
                }

                std::unique_lock<std::mutex> guard(scr->screen_mutex);

                if (!scr->dsa_texture) {
                    const int max_square_width = common::max<int>(screen_size.x, screen_size.y);

                    kern->unlock();
                    guard.unlock();

                    drivers::handle bitmap_handle = drivers::create_bitmap(driver, eka2l1::vec2(max_square_width, max_square_width), epoc::get_bpp_from_display_mode(scr->disp_mode));

                    kern->lock();
                    guard.lock();

                    scr->dsa_texture = bitmap_handle;
                }

                if (!scr->screen_texture) {
                    scr->set_screen_mode(nullptr, driver, scr->crr_mode);
                }

                eka2l1::drivers::filter_option filter = (kern->get_config()->nearest_neighbor_filtering ? eka2l1::drivers::filter_option::nearest : eka2l1::drivers::filter_option::linear);
                drivers::graphics_command_builder builder;

                // Only one rectangle for now!
                builder.update_bitmap(scr->dsa_texture, data_ptr, buffer_size, { 0, 0 }, screen_size);

                // NOTE: This is a hack for some apps that dont fill alpha
                // TODO: Figure out why or better solution (maybe the display mode is not really correct?)
                switch (scr->disp_mode) {
                case epoc::display_mode::color16m:
                case epoc::display_mode::color16mu:
                case epoc::display_mode::color16ma:
                    builder.set_swizzle(scr->dsa_texture, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                        drivers::channel_swizzle::blue, drivers::channel_swizzle::one);

                    break;

                default:
                    break;
                }

                // 270 rotation clock-wise makes screen content comes from the top where camera lies, to down where the ports reside.
                // That makes it a standard, non-flip landscape. 0 is obviously standard too. Therefore mode 90 and 180 needs flip.
                const float rotation_draw = ((mode_info.rotation == 90) || (mode_info.rotation == 180)) ? 180.0f : 0.0f;

                eka2l1::rect source_rect { eka2l1::vec2(0, 0), mode_info.size };
                eka2l1::rect dest_rect = source_rect;
                if (rotation_draw != 0.0f) {
                    // Advance position for rotation origin. We can't gurantee the origin to be exactly div by 2.
                    // So do this for safety and soverginity.
                    dest_rect.top = dest_rect.size;
                }

                dest_rect.scale(scr->display_scale_factor);

                builder.bind_bitmap(scr->screen_texture);
                builder.set_feature(drivers::graphics_feature::clipping, false);

                builder.set_texture_filter(scr->dsa_texture, true, filter);
                builder.set_texture_filter(scr->dsa_texture, false, filter);

                builder.draw_bitmap(scr->dsa_texture, 0, dest_rect, source_rect, eka2l1::vec2(0, 0), rotation_draw, 0);
                builder.bind_bitmap(0);

                drivers::command_list retrieved = builder.retrieve_command_list();
                driver->submit_command_list(retrieved);

                if (((scr->flags_ & epoc::screen::FLAG_SCREEN_UPSCALE_FACTOR_LOCK) == 0) && scr->sync_screen_buffer) {    
                    // The app/game updates normally, try to avoid upscaling it
                    // Sometimes UI are mixed in, and these syncs need to sync UI's data too!
                    // Automatically flag and save this settings
                    if (scr->focus) {
                        scr->focus->saved_setting.screen_upscale_method = 1;
                        scr->restore_from_config(driver, scr->focus->saved_setting);
                        scr->try_change_display_rescale(driver, scr->display_scale_factor);
                    }
                }

                scr->fire_screen_redraw_callbacks(true);
            }

            scr = scr->next;
        }
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, wait_vsync, const std::int32_t screen_index, eka2l1::ptr<epoc::request_status> sts) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();

        epoc::screen *scr = dispatcher->winserv_->get_screens();
        dispatch::screen_post_transferer &transferer = dispatcher->get_screen_post_transferer();

        while (scr != nullptr) {
            if (scr->number == screen_index) {
                epoc::notify_info info{ sts, kern->crr_thread() };

                transferer.wait_vsync(scr, info);
                break;
            }

            scr = scr->next;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(void, cancel_wait_vsync, const std::int32_t screen_index, eka2l1::ptr<epoc::request_status> sts) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();
        dispatch::screen_post_transferer &transferer = dispatcher->get_screen_post_transferer();
        epoc::notify_info info{ sts, kern->crr_thread() };

        transferer.cancel_wait_vsync(info);
    }

    BRIDGE_FUNC_DISPATCHER(void, flexible_post, std::int32_t screen_index, std::uint8_t *data, std::int32_t size_x,
        std::int32_t size_y, std::int32_t format, posting_info *info) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::graphics_driver *driver = sys->get_graphics_driver();
        kernel_system *kern = sys->get_kernel_system();

        epoc::screen *scr = dispatcher->winserv_->get_screens();
        dispatch::screen_post_transferer &transferer = dispatcher->get_screen_post_transferer();

        posting_info post_info_copy = *info;

        post_info_copy.input_crop.transform_from_symbian_rectangle();
        post_info_copy.displayed_rect.transform_from_symbian_rectangle();
        post_info_copy.scale_to_rect.transform_from_symbian_rectangle();

        drivers::graphics_command_builder builder;

        while (scr != nullptr) {
            if (scr->number == screen_index) {
                drivers::handle temp = transferer.transfer_data_to_texture(driver, builder,
                    screen_index, data, eka2l1::vec2(size_x, size_y), format);

                eka2l1::drivers::filter_option filter = (kern->get_config()->nearest_neighbor_filtering ? eka2l1::drivers::filter_option::nearest : eka2l1::drivers::filter_option::linear);
                builder.set_texture_filter(temp, true, filter);
                builder.set_texture_filter(temp, false, filter);

                eka2l1::rect scaled_twice_rect = info->scale_to_rect;
                scaled_twice_rect.scale(scr->display_scale_factor);

                builder.bind_bitmap(scr->screen_texture);
                builder.set_feature(drivers::graphics_feature::clipping, false);
                builder.draw_bitmap(temp, 0, scaled_twice_rect, info->input_crop, eka2l1::vec2(0, 0), 0, 0);
                builder.bind_bitmap(0);

                drivers::command_list retrieved = builder.retrieve_command_list();
                driver->submit_command_list(retrieved);
                scr->fire_screen_redraw_callbacks(true);
            }

            scr = scr->next;
        }
    }

    BRIDGE_FUNC_DISPATCHER(void, fast_blit, fast_blit_info *info) {
        kernel::process *crr_process = sys->get_kernel_system()->crr_process();
        info->src_blit_rect.transform_from_symbian_rectangle();

        std::uint8_t *dest_blt = info->dest_base.get(crr_process);
        const std::uint8_t *src_blt = info->src_base.get(crr_process);

        if ((info->src_size.x == info->src_blit_rect.size.x) && (info->dest_stride == info->src_stride)
            && (info->src_blit_rect.top.x == 0) && (info->dest_point.x == 0)) {
            std::memcpy(dest_blt + info->dest_point.y * info->dest_stride, src_blt + info->src_blit_rect.top.y * info->src_stride,
                info->src_stride * info->src_blit_rect.size.y);
            return;
        }

        // Copy line by line, gurantee same mode already
        const std::int32_t bytes_per_pixel = info->src_stride / info->src_size.x;
        const std::uint32_t bytes_to_copy_per_line = bytes_per_pixel * info->src_blit_rect.size.x;

        for (int y = 0; y < info->src_blit_rect.size.y; y++) {
            std::memcpy(dest_blt + (info->dest_point.y + y) * info->dest_stride + info->dest_point.x * bytes_per_pixel,
                src_blt + (info->src_blit_rect.top.y + y) * info->src_stride + info->src_blit_rect.top.x * bytes_per_pixel,
                bytes_to_copy_per_line);
        }
    }
}
