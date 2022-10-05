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

#include <dispatch/audio.h>
#include <dispatch/dispatcher.h>
#include <vfs/vfs.h>

#include <drivers/audio/audio.h>
#include <system/epoc.h>

#include <kernel/kernel.h>
#include <utils/err.h>
#include <utils/reqsts.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

#include <common/time.h>

#include <utils/des.h>

#include <thread>
#include <chrono>

using namespace std::literals::chrono_literals;

namespace eka2l1::dispatch {
    dsp_epoc_audren_sema::dsp_epoc_audren_sema()
        : own_(0) {
    }

    bool dsp_epoc_audren_sema::destroy() const {
        return !own_.load();
    }

    // Argument reserved
    void dsp_epoc_audren_sema::acquire(void * /* owner */) {
        own_++;
    }

    void dsp_epoc_audren_sema::release(void * /* owner */) {
        own_--;
    }

    dsp_manager::dsp_manager()
        : audren_sema_(nullptr) {
        audren_sema_ = std::make_unique<dsp_epoc_audren_sema>();
    }

    dsp_medium::dsp_medium(dsp_manager *manager, const dsp_medium_type type)
        : manager_(manager)
        , logical_volume_(10)
        , type_(type) {
    }

    dsp_epoc_stream::dsp_epoc_stream(std::unique_ptr<drivers::dsp_stream> &stream, dsp_manager *manager)
        : dsp_medium(manager, DSP_MEDIUM_TYPE_EPOC_STREAM)
        , ll_stream_(std::move(stream)) {
    }

    dsp_epoc_stream::~dsp_epoc_stream() {
    }

    void dsp_epoc_stream::volume(const std::uint32_t volume) {
        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*ll_stream_);
        logical_volume_ = volume;

        out_stream.volume(logical_volume_);
    }

    std::uint32_t dsp_epoc_stream::max_volume() const {
        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*ll_stream_);
        return out_stream.max_volume();
    }

    dsp_epoc_player::dsp_epoc_player(dsp_manager *manager, const std::uint32_t init_flags)
        : dsp_medium(manager, DSP_MEDIUM_TYPE_EPOC_PLAYER)
        , flags_(init_flags)
        , stored_repeat_times(0)
        , stored_trailing_silence_us(0) {
    }

    dsp_epoc_player::~dsp_epoc_player() {
    }

    void dsp_epoc_player::volume(const std::uint32_t volume) {
        logical_volume_ = volume;
        if (impl_) {
            impl_->set_volume(logical_volume_);
        }
    }

    std::uint32_t dsp_epoc_player::max_volume() const {
        if (impl_) {
            return impl_->max_volume();
        }
        return 0;
    }

    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_player_inst, const std::uint32_t init_flags) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        std::unique_ptr<dsp_medium> player_epoc = std::make_unique<dsp_epoc_player>(&manager, init_flags);

        return manager.add_object(player_epoc);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();

        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (eplayer->impl_ && eplayer->impl_->is_playing()) {
            eplayer->impl_->stop();
            manager.audio_renderer_semaphore()->release(eplayer);
        }

        if (!manager.remove_object(handle.ptr_address())) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_supply_url, eka2l1::ptr<void> handle,
        const std::uint16_t *url, const std::uint32_t url_length, std::uint64_t *duration_us) {
        std::u16string url_str(reinterpret_cast<const char16_t *>(url), url_length);

        // Check if the URL references to a local path (drive)
        const std::u16string root = eka2l1::root_path(url_str, true);

        if ((root.length() >= 2) && (root[1] == u':')) {
            // It should be a local path. Transform it to host path
            std::optional<std::u16string> host_path = sys->get_io_system()->get_raw_path(url_str);

            if (host_path.has_value()) {
                url_str = std::move(host_path.value());
            }
        }

        // Proceed with the current URL
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        const std::string url_u8 = common::ucs2_to_utf8(url_str);

        if (!eplayer->impl_ || !eplayer->impl_->open_url(url_u8)) {
            drivers::audio_driver *driver = sys->get_audio_driver();
            std::vector<drivers::player_type> types = driver->get_suitable_player_types(url_u8);

            for (std::size_t i = 0; i < types.size(); i++) {
                if (eplayer->impl_ && (eplayer->impl_->get_player_type() == types[i])) {
                    continue;
                }
                auto new_player = drivers::new_audio_player(driver, types[i]);
                if (new_player) {
                    bool open_res = new_player->open_url(url_u8);
                    
                    if (!eplayer->impl_ || open_res)
                        eplayer->impl_ = std::move(new_player);

                    if (open_res)
                        break;
                }
            }
        }

        // Restore old stream values
        eplayer->impl_->set_volume(eplayer->volume());
        eplayer->impl_->set_repeat(eplayer->stored_repeat_times, eplayer->stored_trailing_silence_us);

        if (duration_us) {
            *duration_us = eplayer->impl_->duration();
        } else {
            LOG_WARN(HLE_DISPATCHER, "Audio duration pointer is supposed to not be null!");
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_supply_buffer, eka2l1::ptr<void> handle, epoc::des8 *buffer, std::uint64_t *duration_us) {
        kernel::process *pr = sys->get_kernel_system()->crr_process();

        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->custom_stream_ = std::make_unique<epoc::rw_des_stream>(buffer, pr);
        common::rw_stream *custom_good_stream = reinterpret_cast<common::rw_stream *>(eplayer->custom_stream_.get());

        if (!eplayer->impl_ || !eplayer->impl_->open_custom(custom_good_stream)) {
            drivers::audio_driver *driver = sys->get_audio_driver();
            std::vector<drivers::player_type> types = driver->get_suitable_player_types("");

            for (std::size_t i = 0; i < types.size(); i++) {
                if (eplayer->impl_ && (eplayer->impl_->get_player_type() == types[i])) {
                    continue;
                }
                auto new_player = drivers::new_audio_player(driver, types[i]);
                if (new_player) {
                    const bool open_res = new_player->open_custom(custom_good_stream);
                    if (open_res || !eplayer->impl_) {
                        eplayer->impl_ = std::move(new_player);
                    }

                    if (open_res)
                        break;
                }
            }
        }

        // Restore old stream values
        eplayer->impl_->set_volume(eplayer->volume());
        eplayer->impl_->set_repeat(eplayer->stored_repeat_times, eplayer->stored_trailing_silence_us);

        if (duration_us) {
            *duration_us = eplayer->impl_->duration();
        } else {
            LOG_WARN(HLE_DISPATCHER, "Audio duration pointer is supposed to not be null!");
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_play, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->play()) {
            return epoc::error_general;
        }

        manager.audio_renderer_semaphore()->acquire(eplayer);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_stop, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (eplayer->impl_) {
            if (!eplayer->impl_->stop()) {
                return epoc::error_general;
            }

            manager.audio_renderer_semaphore()->destroy();
        }

        return (sys->get_symbian_version_use() <= epocver::eka2) ? 1 : epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_pause, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (eplayer->impl_)
            eplayer->impl_->pause();

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_max_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return eplayer->max_volume();
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return eplayer->volume();
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_volume, eka2l1::ptr<void> handle, const std::int32_t volume) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (volume > eplayer->max_volume()) {
            return epoc::error_argument;
        }

        eplayer->volume(volume);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_notify_any_done, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> sts) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        epoc::notify_info info;

        info.requester = sys->get_kernel_system()->crr_thread();
        info.sts = sts;

        if (!eplayer->impl_->notify_any_done([eplayer](std::uint8_t *data) {
                epoc::notify_info *info = reinterpret_cast<epoc::notify_info *>(data);
                kernel_system *kern = info->requester->get_kernel_object_owner();

                kern->lock();
                info->complete(epoc::error_none);
                kern->unlock();

                eplayer->impl_->clear_notify_done();
            },
                reinterpret_cast<std::uint8_t *>(&info), sizeof(epoc::notify_info))) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_cancel_notify_done, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(eplayer->impl_->lock_);
        std::uint8_t *notify = eplayer->impl_->get_notify_userdata(nullptr);

        if (!notify) {
            return epoc::error_none;
        }

        reinterpret_cast<epoc::notify_info *>(notify)->complete(epoc::error_cancel);
        eplayer->impl_->clear_notify_done();

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_repeats, eka2l1::ptr<void> handle, const std::int32_t times, const std::uint32_t silence_interval_micros_low,
        const std::int32_t silence_micros_high) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->stored_repeat_times = times;
        eplayer->stored_trailing_silence_us = silence_interval_micros_low | static_cast<std::int64_t>(silence_micros_high << 32);

        if (eplayer->impl_)
            eplayer->impl_->set_repeat(times, eplayer->stored_trailing_silence_us);

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_dest_sample_rate, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(eplayer->impl_->get_dest_freq());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_sample_rate, eka2l1::ptr<void> handle, const std::int32_t sample_rate) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_dest_freq(sample_rate)) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_dest_channel_count, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(eplayer->impl_->get_dest_channel_count());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_channel_count, eka2l1::ptr<void> handle, const std::int32_t channel_count) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_dest_channel_count(channel_count)) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_dest_encoding, eka2l1::ptr<void> handle, std::uint32_t *encoding) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!encoding) {
            return epoc::error_argument;
        }

        *encoding = eplayer->impl_->get_dest_encoding();
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_encoding, eka2l1::ptr<void> handle, const std::uint32_t encoding) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!eplayer->impl_->set_dest_encoding(encoding)) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_dest_container_format, eka2l1::ptr<void> handle, const std::uint32_t container_format) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        eplayer->impl_->set_dest_container_format(container_format);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_set_position, eka2l1::ptr<void> handle, std::uint64_t pos_in_micros) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (eplayer->impl_)
            eplayer->impl_->set_position(pos_in_micros);

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_player_get_position, eka2l1::ptr<void> handle, std::uint64_t *pos_get) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_player *eplayer = manager.get_object<dsp_epoc_player>(handle.ptr_address());

        if (!eplayer) {
            return epoc::error_bad_handle;
        }

        if (!pos_get) {
            return epoc::error_argument;
        }

        *pos_get = eplayer->impl_->position();
        return epoc::error_none;
    }

    eka2l1::ptr<void> eaudio_dsp_stream_create_impl(system *sys, bool in_stream) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::audio_driver *aud_driver = sys->get_audio_driver();

        auto ll_stream = in_stream ? drivers::new_dsp_in_stream(aud_driver, drivers::dsp_stream_backend_ffmpeg)
            : drivers::new_dsp_out_stream(aud_driver, drivers::dsp_stream_backend_ffmpeg);

        if (!ll_stream) {
            LOG_ERROR(HLE_AUD, "Unable to create new DSP out stream!");
            return 0;
        }

        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        std::unique_ptr<dsp_medium> stream_new = std::make_unique<dsp_epoc_stream>(ll_stream, &manager);

        dsp_epoc_stream *stream_org_new = reinterpret_cast<dsp_epoc_stream *>(stream_new.get());

        stream_org_new->ll_stream_->register_callback(
            drivers::dsp_stream_notification_more_buffer, [](void *userdata) {
                dsp_epoc_stream *epoc_stream = reinterpret_cast<dsp_epoc_stream *>(userdata);
                const std::lock_guard<std::mutex> guard(epoc_stream->lock_);

                epoc::notify_info &info = epoc_stream->copied_info_;

                if (!info.empty()) {
                    kernel_system *kern = info.requester->get_kernel_object_owner();

                    kern->lock();
                    info.complete(epoc::error_none);
                    kern->unlock();
                }
            },
            stream_new.get());

        return manager.add_object(stream_new);
    }

    // DSP streams
    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_dsp_out_stream_create, void *) {
        return eaudio_dsp_stream_create_impl(sys, false);
    }

    BRIDGE_FUNC_DISPATCHER(eka2l1::ptr<void>, eaudio_dsp_in_stream_create, void *) {
        return eaudio_dsp_stream_create_impl(sys, true);
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_destroy, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (stream->ll_stream_->is_playing() || stream->ll_stream_->is_recording()) {
            stream->ll_stream_->stop();
            manager.audio_renderer_semaphore()->release(stream);
        }

        manager.remove_object(handle.ptr_address());
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_set_properties, eka2l1::ptr<void> handle, const std::uint32_t freq, const std::uint32_t channels) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->set_properties(freq, channels)) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_start, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->start()) {
            return epoc::error_general;
        }

        manager.audio_renderer_semaphore()->acquire(stream);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_stop, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->stop()) {
            return epoc::error_general;
        }

        // Return value >= 0 indicates the behaviour of calling MaoscPlayComplete. 0 means call.
        // See the impl.cpp file for a more detailed explaination on why
        if (sys->get_symbian_version_use() <= epocver::epoc6) {
            return 1;
        }

        manager.audio_renderer_semaphore()->release(stream);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_write, eka2l1::ptr<void> handle, const std::uint8_t *data, const std::uint32_t data_size) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        drivers::dsp_output_stream &out_stream = static_cast<drivers::dsp_output_stream &>(*stream->ll_stream_);

        if (!out_stream.write(data, data_size)) {
            return epoc::error_not_ready;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready, eka2l1::ptr<void> handle, eka2l1::ptr<epoc::request_status> req) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(stream->lock_);

        stream->copied_info_ = epoc::notify_info{ req, sys->get_kernel_system()->crr_thread() };
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_notify_buffer_ready_cancel, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        const std::lock_guard<std::mutex> guard(stream->lock_);

        stream->copied_info_.complete(epoc::error_cancel);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_max_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(stream->max_volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_volume, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(stream->volume());
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_out_stream_set_volume, eka2l1::ptr<void> handle, std::uint32_t vol) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (vol > stream->max_volume()) {
            return epoc::error_argument;
        }

        stream->volume(vol);
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_bytes_rendered, eka2l1::ptr<void> handle, std::uint64_t *bytes) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        *bytes = stream->ll_stream_->bytes_rendered();
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_position, eka2l1::ptr<void> handle, std::uint64_t *the_time) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        kernel_system *kern = sys->get_kernel_system();

        if (kern->is_eka1()) {
            // Simulate IPC latency
            std::this_thread::sleep_for(1us);
        }

        // Well weird enough this position is calculated from the total sample copied
        // This is related to THPS. Where it checks for the duration when the buffer has just been copied.
        // If the duration played drains the buffer, only at that time it starts to supply more audio data.
        *the_time = stream->ll_stream_->real_time_position();
        return epoc::error_none;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_reset_stat, eka2l1::ptr<void> handle) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        stream->ll_stream_->reset_stat();
        return epoc::error_none;
    }
    
    BRIDGE_FUNC_DISPATCHER(std::int32_t, eaudio_dsp_stream_set_format, eka2l1::ptr<void> handle, const std::int32_t four_cc) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        dispatch::dsp_manager &manager = dispatcher->get_dsp_manager();
        dsp_epoc_stream *stream = manager.get_object<dsp_epoc_stream>(handle.ptr_address());

        if (!stream) {
            return epoc::error_bad_handle;
        }

        if (!stream->ll_stream_->format(four_cc)) {
            return epoc::error_not_supported;
        }

        return epoc::error_none;
    }
}
