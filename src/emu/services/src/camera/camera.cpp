/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <services/camera/camera.h>
#include <services/fbs/fbs.h>

#include <common/log.h>
#include <drivers/camera/camera_collection.h>
#include <kernel/kernel.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace eka2l1 {
    namespace {
        constexpr const char *CAMERA_SERVER_NAME = "CameraServer";

        enum camera_opcode {
            camera_turn_on = 0,
            camera_turn_off = 1,
            camera_set_lighting = 2,
            camera_set_image_quality = 3,
            camera_get_image = 4
        };
    }

    struct camera_session::capture_state {
        system *sys;
        epoc::notify_info notify;
        fbs_server *fbs;
        fbsbitmap *bitmap;
        std::uint32_t bitmap_handle;
        bool owns_bitmap;
        eka2l1::vec2 source_size;
        eka2l1::vec2 target_size;
        epoc::display_mode target_mode;
        std::uint32_t source_stride;
        std::uint32_t target_stride;
        std::uint32_t bytes_per_pixel;
        std::atomic<bool> cancelled{ false };
        std::atomic<bool> done{ false };
    };

    // Frames arrive on a driver thread for as long as the feed runs, so the state
    // the callback touches outlives the session it was started from.
    struct camera_session::feed_state {
        system *sys;

        // Only read/written with the kernel lock held.
        std::shared_ptr<capture_state> pending;

        std::atomic<bool> wants_frame{ false };
        std::atomic<bool> stopped{ false };
        std::atomic<bool> failed{ false };
    };

    camera_session::camera_session(service::typical_server *server, const kernel::uid client_session_uid,
        const epoc::version client_version)
        : service::typical_session(server, client_session_uid, client_version)
        , low_quality_(false)
        , night_mode_(false)
        , feed_size_(0, 0)
        , feed_format_(0) {
    }

    camera_session::~camera_session() {
        turn_off();
    }

    int camera_session::turn_on(system *sys) {
        if (camera_) {
            return epoc::error_none;
        }

        drivers::camera::collection *collection = drivers::camera::get_collection();
        if (!collection || (collection->count() == 0)) {
            return epoc::error_not_supported;
        }

        // The iOS backend may synchronously ask the user for camera permission.
        // Do not hold the guest kernel while the main thread displays that prompt.
        kernel_system *kern = sys->get_kernel_system();
        kern->unlock();
        std::unique_ptr<drivers::camera::instance> camera = collection->make_camera(0);
        kern->lock();

        if (!camera) {
            return epoc::error_general;
        }

        if (!camera->reserve()) {
            return epoc::error_in_use;
        }

        camera->set_parameter(drivers::camera::PARAMETER_KEY_EXPOSURE,
            night_mode_ ? drivers::camera::EXPOSURE_MODE_NIGHT : drivers::camera::EXPOSURE_MODE_AUTO);
        camera_ = std::move(camera);
        return epoc::error_none;
    }

    void camera_session::turn_off() {
        stop_feed();

        if (pending_capture_) {
            const std::shared_ptr<capture_state> state = pending_capture_;
            state->cancelled.store(true);

            if (!state->done.exchange(true)) {
                kernel_system *kern = state->sys->get_kernel_system();
                if (!kern->is_wiping() && state->owns_bitmap
                    && state->bitmap && (state->bitmap->count == 0)) {
                    state->fbs->remove(state->bitmap);
                    state->bitmap = nullptr;
                }

                if (!kern->is_wiping() && kern->is_thread_alive(state->notify.requester)) {
                    state->notify.complete(epoc::error_cancel);
                }
            }

            pending_capture_.reset();
        }

        if (camera_) {
            camera_->release();
            camera_.reset();
        }
    }

    void camera_session::stop_feed() {
        if (!feed_) {
            return;
        }

        // Retire the shared state before the driver so that a frame already in
        // flight on the driver thread finds the feed stopped and does nothing.
        feed_->stopped.store(true);
        feed_->wants_frame.store(false);
        feed_->pending.reset();
        feed_.reset();

        feed_size_ = eka2l1::vec2(0, 0);
        feed_format_ = 0;

        if (camera_) {
            camera_->stop_viewfinder_feed();
        }

        LOG_TRACE(HLE_CAMERA, "Camera frame feed stopped");
    }

    bool camera_session::start_feed(system *sys, const eka2l1::vec2 &size, const std::uint32_t format,
        const std::shared_ptr<capture_state> &first_request) {
        std::shared_ptr<feed_state> feed = std::make_shared<feed_state>();
        feed->sys = sys;
        feed->pending = first_request;
        feed->wants_frame.store(true);

        feed_ = feed;
        feed_size_ = size;
        feed_format_ = format;

        kernel_system *kern = sys->get_kernel_system();

        // Drivers normally deliver on their own thread, but a feed that fails to
        // start reports it through the same callback, synchronously. Let that call
        // take the kernel lock instead of deadlocking against this thread.
        kern->unlock();
        camera_->receive_viewfinder_feed(size, static_cast<drivers::camera::frame_format>(format),
            [feed]() {
                return feed->wants_frame.load() && !feed->stopped.load();
            },
            [feed](const void *buffer, const std::size_t buffer_size, const int error) {
                if (feed->stopped.load() || !feed->wants_frame.load()) {
                    return;
                }

                kernel_system *kern = feed->sys->get_kernel_system();
                kernel_lock guard(kern);

                if (feed->stopped.load() || kern->is_wiping()) {
                    return;
                }

                if (error < 0) {
                    feed->failed.store(true);
                }

                const std::shared_ptr<capture_state> state = feed->pending;
                feed->pending.reset();
                feed->wants_frame.store(false);

                if (state) {
                    deliver_frame(state, buffer, buffer_size, error);
                }
            });
        kern->lock();

        // A feed that cannot start reports it through the same callback, which has
        // already completed the request by now. Tear the dead feed down so that the
        // next request tries again instead of waiting forever.
        if (feed->failed.load()) {
            LOG_ERROR(HLE_CAMERA, "Unable to start the camera frame feed!");
            stop_feed();
            return false;
        }

        LOG_TRACE(HLE_CAMERA, "Camera frame feed started at {}x{}, format 0x{:X}",
            size.x, size.y, format);
        return true;
    }

    void camera_session::deliver_frame(const std::shared_ptr<capture_state> &state, const void *buffer,
        const std::size_t buffer_size, const int error) {
        kernel_system *kern = state->sys->get_kernel_system();

        if (state->cancelled.load() || state->done.exchange(true)) {
            return;
        }

        if (!kern->is_thread_alive(state->notify.requester)) {
            if (state->owns_bitmap && state->bitmap && (state->bitmap->count == 0)) {
                state->fbs->remove(state->bitmap);
                state->bitmap = nullptr;
            }

            return;
        }

        int result = epoc::error_none;
        fbs_server *fbs = state->fbs;
        fbsbitmap *bitmap = fbs ? fbs->get<fbsbitmap>(state->bitmap_handle) : nullptr;

        if ((error < 0) || !buffer) {
            result = epoc::error_general;
        } else if (!bitmap || (bitmap->kind != fbsobj_kind::bitmap)) {
            result = epoc::error_bad_handle;
        } else {
            epoc::display_mode mode = bitmap->bitmap_->settings_.current_display_mode();
            if (mode == epoc::display_mode::none) {
                mode = bitmap->bitmap_->settings_.initial_display_mode();
            }

            const std::size_t source_bytes = static_cast<std::size_t>(state->source_stride)
                * state->source_size.y;
            const std::size_t target_bytes = static_cast<std::size_t>(state->target_stride)
                * state->target_size.y;

            if ((bitmap->bitmap_->header_.size_pixels != state->target_size)
                || (mode != state->target_mode)
                || (bitmap->bitmap_->data_size() < target_bytes)
                || (buffer_size < source_bytes)) {
                result = epoc::error_underflow;
            } else {
                const std::uint8_t *source = reinterpret_cast<const std::uint8_t *>(buffer);
                std::uint8_t *target = bitmap->bitmap_->data_pointer(fbs);

                if ((state->source_size == state->target_size)
                    && (state->source_stride == state->target_stride)) {
                    std::memcpy(target, source, target_bytes);
                } else {
                    std::memset(target, 0, target_bytes);

                    for (int y = 0; y < state->target_size.y; y++) {
                        const int source_y = y * state->source_size.y / state->target_size.y;
                        for (int x = 0; x < state->target_size.x; x++) {
                            const int source_x = x * state->source_size.x / state->target_size.x;
                            std::memcpy(target + static_cast<std::size_t>(y) * state->target_stride
                                    + static_cast<std::size_t>(x) * state->bytes_per_pixel,
                                source + static_cast<std::size_t>(source_y) * state->source_stride
                                    + static_cast<std::size_t>(source_x) * state->bytes_per_pixel,
                                state->bytes_per_pixel);
                        }
                    }
                }
            }
        }

        if ((result != epoc::error_none) && state->owns_bitmap
            && state->bitmap && (state->bitmap->count == 0)) {
            state->fbs->remove(state->bitmap);
            state->bitmap = nullptr;
        }

        state->notify.complete(result);
    }

    void camera_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case camera_turn_on:
            ctx->complete(turn_on(ctx->sys));
            break;

        case camera_turn_off:
            turn_off();
            ctx->complete(epoc::error_none);
            break;

        case camera_set_lighting: {
            const std::optional<std::int32_t> lighting = ctx->get_argument_value<std::int32_t>(0);
            if (!lighting || ((*lighting != 0) && (*lighting != 1))) {
                ctx->complete(epoc::error_argument);
                break;
            }

            night_mode_ = (*lighting == 1);
            LOG_TRACE(HLE_CAMERA, "Camera night mode set to {}", night_mode_);
            if (camera_) {
                camera_->set_parameter(drivers::camera::PARAMETER_KEY_EXPOSURE,
                    night_mode_ ? drivers::camera::EXPOSURE_MODE_NIGHT : drivers::camera::EXPOSURE_MODE_AUTO);
            }

            ctx->complete(epoc::error_none);
            break;
        }

        case camera_set_image_quality: {
            const std::optional<std::int32_t> quality = ctx->get_argument_value<std::int32_t>(0);
            if (!quality || ((*quality != 0) && (*quality != 1))) {
                ctx->complete(epoc::error_argument);
                break;
            }

            low_quality_ = (*quality == 1);
            LOG_TRACE(HLE_CAMERA, "Camera low quality mode set to {}", low_quality_);
            ctx->complete(epoc::error_none);
            break;
        }

        case camera_get_image: {
            if (!camera_) {
                ctx->complete(epoc::error_not_ready);
                break;
            }

            if (pending_capture_ && !pending_capture_->done.load()) {
                ctx->complete(epoc::error_in_use);
                break;
            }
            pending_capture_.reset();

            kernel_system *kern = ctx->sys->get_kernel_system();
            fbs_server *fbs = reinterpret_cast<fbs_server *>(kern->get_by_name<service::server>(
                epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version())));
            if (!fbs) {
                ctx->complete(epoc::error_not_ready);
                break;
            }

            // S60v1/N-Gage and S60v2 FP3 ship two incompatible revisions of
            // the same client DLL. The older client creates the bitmap and
            // sends its global FBS handle in slot 0 plus the Create() result
            // in slot 1. FP3 sends a four-byte output package in slot 0; the
            // server creates the bitmap and the client duplicates the handle
            // after completion. A valid FBS global handle distinguishes the
            // old form from a guest descriptor address without device hacks.
            const std::optional<std::uint32_t> raw_slot0 = ctx->get_argument_value<std::uint32_t>(0);
            const std::optional<std::int32_t> create_result = ctx->get_argument_value<std::int32_t>(1);
            fbsbitmap *bitmap = raw_slot0 ? fbs->get<fbsbitmap>(*raw_slot0) : nullptr;
            bool owns_bitmap = false;

            if (!bitmap && raw_slot0 && (*raw_slot0 == 0)
                && create_result && (*create_result < epoc::error_none)) {
                ctx->complete(*create_result);
                break;
            }

            if (bitmap && (bitmap->kind != fbsobj_kind::bitmap)) {
                bitmap = nullptr;
            }

            if (!bitmap && (!ctx->get_descriptor_argument_ptr(0)
                    || (ctx->get_argument_max_data_size(0) < sizeof(std::uint32_t)))) {
                ctx->complete(epoc::error_bad_descriptor);
                break;
            }

            const eka2l1::vec2 expected_size = low_quality_ ? eka2l1::vec2(160, 120) : eka2l1::vec2(640, 480);
            const epoc::display_mode expected_mode = low_quality_
                ? epoc::display_mode::color4k : epoc::display_mode::color16m;
            const drivers::camera::frame_format format = low_quality_
                ? drivers::camera::FRAME_FORMAT_FBSBMP_COLOR4K
                : drivers::camera::FRAME_FORMAT_FBSBMP_COLOR16M;
            const std::uint32_t bits_per_pixel = low_quality_ ? 12 : 24;
            const std::uint32_t bytes_per_pixel = low_quality_ ? 2 : 3;

            // Frames come from the viewfinder feed, which delivers exactly the
            // requested size in the requested guest format. A driver that does not
            // know the format would drop the feed request without reporting it, so
            // reject it here rather than leaving the client waiting forever.
            const std::vector<drivers::camera::frame_format> formats = camera_->supported_frame_formats();
            if (std::find(formats.begin(), formats.end(), format) == formats.end()) {
                ctx->complete(epoc::error_not_supported);
                break;
            }

            const eka2l1::vec2 source_size = expected_size;

            if (!bitmap) {
                bool support_current_display_mode = true;
                bool support_dirty_bitmap = true;
                epoc::query_fbs_feature_support(fbs, support_current_display_mode, support_dirty_bitmap);

                fbs_bitmap_data_info bitmap_info;
                bitmap_info.size_ = expected_size;
                bitmap_info.dpm_ = expected_mode;
                bitmap = fbs->create_bitmap(bitmap_info, true,
                    support_current_display_mode, support_dirty_bitmap);
                if (!bitmap) {
                    ctx->complete(epoc::error_no_memory);
                    break;
                }

                owns_bitmap = true;
                if (!ctx->write_data_to_descriptor_argument(0, bitmap->id)) {
                    fbs->remove(bitmap);
                    ctx->complete(epoc::error_bad_descriptor);
                    break;
                }
            }

            epoc::display_mode bitmap_mode = bitmap->bitmap_->settings_.current_display_mode();
            if (bitmap_mode == epoc::display_mode::none) {
                bitmap_mode = bitmap->bitmap_->settings_.initial_display_mode();
            }
            if ((bitmap->bitmap_->header_.size_pixels != expected_size)
                || (bitmap_mode != expected_mode)) {
                if (owns_bitmap) {
                    fbs->remove(bitmap);
                }
                ctx->complete(epoc::error_argument);
                break;
            }

            const std::uint32_t bitmap_handle = bitmap->id;
            const std::uint32_t source_stride = epoc::get_byte_width(source_size.x, bits_per_pixel);
            const std::uint32_t target_stride = static_cast<std::uint32_t>(bitmap->bitmap_->byte_width_);
            const std::size_t target_bytes = static_cast<std::size_t>(target_stride) * expected_size.y;
            if (bitmap->bitmap_->data_size() < target_bytes) {
                if (owns_bitmap) {
                    fbs->remove(bitmap);
                }
                ctx->complete(epoc::error_underflow);
                break;
            }

            std::shared_ptr<capture_state> state = std::make_shared<capture_state>();
            state->sys = ctx->sys;
            state->notify = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
            state->fbs = fbs;
            state->bitmap = bitmap;
            state->bitmap_handle = bitmap_handle;
            state->owns_bitmap = owns_bitmap;
            state->source_size = source_size;
            state->target_size = expected_size;
            state->target_mode = expected_mode;
            state->source_stride = source_stride;
            state->target_stride = target_stride;
            state->bytes_per_pixel = bytes_per_pixel;
            pending_capture_ = state;

            // GetImage is how this protocol implements a viewfinder: apps poll it
            // frame after frame. Serving each poll with a still capture makes the
            // host run its whole photo pipeline (and, on iOS, the shutter sound)
            // per frame, so take frames from the viewfinder feed and leave it
            // running between requests instead.
            if (feed_ && (feed_->failed.load() || (feed_size_ != expected_size)
                || (feed_format_ != static_cast<std::uint32_t>(format)))) {
                // A feed that reported an error stays dead, and the quality
                // setting can change the size and format between requests.
                stop_feed();
            }

            if (feed_) {
                feed_->pending = state;
                feed_->wants_frame.store(true);
            } else {
                start_feed(ctx->sys, expected_size, static_cast<std::uint32_t>(format), state);
            }

            break;
        }

        default:
            ctx->complete(epoc::error_not_supported);
            break;
        }
    }

    camera_server::camera_server(system *sys)
        : service::typical_server(sys, CAMERA_SERVER_NAME) {
    }

    void camera_server::connect(service::ipc_context &ctx) {
        create_session<camera_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

}
