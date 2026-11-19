// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <drivers/audio/audio.h>
#include <drivers/audio/stream.h>
#include <kernel/chunk.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/ldd.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <vector>

namespace eka2l1::ldd {
    namespace {
        constexpr std::uint32_t pcm16 = 1;
        constexpr std::uint32_t min_buffer = 16;
        constexpr std::uint32_t max_buffer = 65536;
        constexpr std::array<std::uint32_t, 9> sample_rates{8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000};

        std::uint32_t sample_rate_from_mask(std::uint32_t mask) {
            for (std::size_t i = 0; i < sample_rates.size(); ++i) {
                if (mask == (1U << i)) {
                    return sample_rates[i];
                }
            }
            return 0;
        }

        enum control : std::uint32_t {
            playback_capabilities = 0,
            playback_get_config = 1,
            playback_set_config = 2,
            playback_open = 3,
            playback_close = 4,
            playback_pause = 5,
            playback_resume = 6,
            playback_bytes = 7,
            cancel_request = 0x7fffffff
        };

        enum request : std::uint32_t {
            playback_submit = 0,
            playback_error = 1
        };

        struct playback_config {
            std::uint32_t format;
            std::uint32_t rate_mask;
            std::uint32_t channels;
            std::uint32_t buffer_size;
            std::uint32_t flags;
        };
        static_assert(sizeof(playback_config) == 20);

        struct playback_buffer {
            std::uint32_t address;
            std::uint32_t length;
            std::array<std::uint32_t, 4> codec_info;
            std::uint32_t last;
            std::uint32_t reserved;
        };
        static_assert(sizeof(playback_buffer) == 32);

        struct pending_request {
            eka2l1::ptr<epoc::request_status> status{};
            kernel::uid thread_id = 0;

            bool empty() const {
                return !status;
            }

            void set(const epoc::notify_info &info) {
                status = info.sts;
                thread_id = info.requester->unique_id();
            }

            kernel::thread *requester(kernel_system *kern) const {
                if (empty() || kern->is_wiping()) {
                    return nullptr;
                }
                auto *thread = kern->get_by_id<kernel::thread>(thread_id);
                return thread && thread->current_state() != kernel::thread_state::stop ? thread : nullptr;
            }

            void complete(kernel_system *kern, std::int32_t result) {
                if (auto *thread = requester(kern)) {
                    epoc::notify_info(status, thread).complete(result);
                }
                status = 0;
            }
        };

        // The LDD exchanges raw structs, not Symbian descriptors.
        template <typename T>
        T *guest_struct(kernel::process *process, eka2l1::ptr<void> addr) {
            const auto start = addr.ptr_address();
            if (!start || start > UINT32_MAX - (sizeof(T) - 1)) {
                return nullptr;
            }
            auto *first = static_cast<std::uint8_t *>(process->get_ptr_on_addr_space(start));
            const auto *last = process->get_ptr_on_addr_space(start + sizeof(T) - 1);
            if (!first || reinterpret_cast<std::uintptr_t>(last) != reinterpret_cast<std::uintptr_t>(first) + sizeof(T) - 1) {
                return nullptr;
            }
            return reinterpret_cast<T *>(first);
        }

        class ext_sound_channel final : public channel {
            playback_config config_{pcm16, 0x100, 2, 48000, 0};
            std::uint32_t rate_ = 48000;
            kernel::chunk *buffer_ = nullptr;
            kernel::process *process_ = nullptr;
            std::unique_ptr<drivers::audio_output_stream> stream_;
            pending_request submit_;
            pending_request error_;
            eka2l1::ptr<void> result_{};
            std::mutex samples_mutex_;
            std::vector<std::int16_t> samples_;
            std::size_t cursor_ = 0;
            std::uint64_t bytes_ = 0;
            bool last_ = false;
            bool paused_ = false;

            playback_buffer buffer_info() const {
                return {buffer_->base(process_).ptr_address(), config_.buffer_size, {}, 0, 0};
            }

            std::size_t render(std::int16_t *destination, std::size_t frames) {
                bool copied_all;
                {
                    std::lock_guard<std::mutex> guard(samples_mutex_);
                    const auto count = frames * config_.channels;
                    const auto copied = std::min(count, samples_.size() - cursor_);
                    if (copied) {
                        std::copy_n(samples_.data() + cursor_, copied, destination);
                    }
                    std::fill_n(destination + copied, count - copied, 0);
                    cursor_ += copied;
                    bytes_ += copied * sizeof(std::int16_t);
                    copied_all = cursor_ == samples_.size();
                }
                if (copied_all) {
                    // Closing the channel stops the host stream under the kernel lock.
                    std::unique_lock<kernel_system> guard(*kern, std::try_to_lock);
                    if (guard.owns_lock() && !kern->is_wiping() && !submit_.empty()) {
                        {
                            std::lock_guard<std::mutex> samples_guard(samples_mutex_);
                            if (cursor_ != samples_.size()) {
                                return frames;
                            }
                        }
                        auto *thread = submit_.requester(kern);
                        auto *value = thread ? guest_struct<playback_buffer>(thread->owning_process(), result_) : nullptr;
                        if (value) {
                            const auto response = buffer_info();
                            std::memcpy(value, &response, sizeof(response));
                        }
                        submit_.complete(kern, value ? epoc::error_none : epoc::error_argument);
                        if (last_) {
                            error_.complete(kern, epoc::error_underflow);
                        }
                    }
                }
                return frames;
            }

            void close(std::int32_t playback_result = epoc::error_cancel) {
                if (stream_) {
                    stream_->stop();
                    stream_.reset();
                }
                submit_.complete(kern, epoc::error_cancel);
                error_.complete(kern, playback_result);
                // Kernel wipeout has already destroyed its chunks by this point.
                if (buffer_ && !kern->is_wiping()) {
                    buffer_->decrease_access_count();
                }
                buffer_ = nullptr;
                process_ = nullptr;
                samples_.clear();
                cursor_ = 0;
                paused_ = false;
            }

            std::int32_t open(kernel::process *process, eka2l1::ptr<void> address) {
                auto *result = guest_struct<playback_buffer>(process, address);
                if (!result) {
                    return epoc::error_argument;
                }
                if (stream_) {
                    return epoc::error_in_use;
                }
                auto *audio = sys_->get_audio_driver();
                if (!audio) {
                    return epoc::error_not_ready;
                }
                buffer_ = kern->create<kernel::chunk>(kern->get_memory_system(), process, "ExtSoundPlayback",
                    0, config_.buffer_size, config_.buffer_size, prot_read_write, kernel::chunk_type::normal,
                    kernel::chunk_access::local, kernel::chunk_attrib::none);
                if (!buffer_) {
                    return epoc::error_no_memory;
                }
                buffer_->increase_access_count();
                process_ = process;
                const auto response = buffer_info();
                if (!response.address || !buffer_->host_base()) {
                    close();
                    return epoc::error_no_memory;
                }
                bytes_ = 0;
                stream_ = audio->new_output_stream(rate_, static_cast<std::uint8_t>(config_.channels),
                    [this](std::int16_t *data, std::size_t frames) { return render(data, frames); });
                if (stream_) {
                    stream_->set_volume(1.0f);
                }
                if (!stream_ || !stream_->start()) {
                    close();
                    return epoc::error_not_ready;
                }
                std::memcpy(result, &response, sizeof(response));
                return epoc::error_none;
            }

        public:
            using channel::channel;
            ~ext_sound_channel() override {
                close();
            }

            std::int32_t do_control(kernel::thread *thread, std::uint32_t op, eka2l1::ptr<void> arg,
                eka2l1::ptr<void>) override {
                auto *process = thread->owning_process();
                switch (op) {
                case playback_capabilities:
                case playback_get_config: {
                    auto *result = guest_struct<playback_config>(process, arg);
                    if (!result) {
                        return epoc::error_argument;
                    }
                    const playback_config caps{pcm16, 0x1ff, 2, min_buffer, max_buffer};
                    const auto &value = op == playback_capabilities ? caps : config_;
                    std::memcpy(result, &value, sizeof(value));
                    return epoc::error_none;
                }
                case playback_set_config: {
                    if (stream_) {
                        return epoc::error_in_use;
                    }
                    const auto *source = guest_struct<playback_config>(process, arg);
                    if (!source) {
                        return epoc::error_argument;
                    }
                    playback_config value;
                    std::memcpy(&value, source, sizeof(value));
                    const auto rate = sample_rate_from_mask(value.rate_mask);
                    if (value.format != pcm16 || !rate
                        || value.channels < 1 || value.channels > 2
                        || value.buffer_size < min_buffer || value.buffer_size > max_buffer
                        || value.buffer_size % (sizeof(std::int16_t) * value.channels)) {
                        return epoc::error_not_supported;
                    }
                    config_ = value;
                    rate_ = rate;
                    return epoc::error_none;
                }
                case playback_open:
                    return open(process, arg);
                case playback_close:
                    close(epoc::error_none);
                    return epoc::error_none;
                case playback_pause:
                    if (stream_) {
                        stream_->pause();
                        paused_ = true;
                    }
                    return epoc::error_none;
                case playback_resume:
                    if (!stream_ || !paused_ || !stream_->start()) {
                        return epoc::error_not_ready;
                    }
                    paused_ = false;
                    return epoc::error_none;
                case playback_bytes: {
                    std::lock_guard<std::mutex> guard(samples_mutex_);
                    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bytes_));
                }
                case cancel_request:
                    if (arg.ptr_address() == playback_error) {
                        error_.complete(kern, epoc::error_cancel);
                    } else if (arg.ptr_address() == playback_submit) {
                        submit_.complete(kern, epoc::error_cancel);
                    }
                    return epoc::error_none;
                default:
                    return epoc::error_not_supported;
                }
            }

            std::int32_t do_request(epoc::notify_info info, std::uint32_t op, eka2l1::ptr<void> arg,
                eka2l1::ptr<void>, bool) override {
                if (info.empty()) {
                    return epoc::error_argument;
                }
                std::int32_t result = epoc::error_not_supported;
                if (op == playback_error) {
                    if (error_.empty()) {
                        error_.set(info);
                        return epoc::error_none;
                    }
                    result = epoc::error_in_use;
                } else if (op == playback_submit) {
                    if (!stream_ || info.requester->owning_process() != process_) {
                        result = epoc::error_not_ready;
                    } else if (!submit_.empty()) {
                        result = epoc::error_in_use;
                    } else if (const auto *value = guest_struct<playback_buffer>(process_, arg)) {
                        playback_buffer data;
                        std::memcpy(&data, value, sizeof(data));
                        const auto buffer = buffer_info();
                        if (data.address == buffer.address && data.length <= buffer.length
                            && data.length % (sizeof(std::int16_t) * config_.channels) == 0) {
                            std::lock_guard<std::mutex> guard(samples_mutex_);
                            samples_.resize(data.length / sizeof(std::int16_t));
                            if (data.length) {
                                std::memcpy(samples_.data(), buffer_->host_base(), data.length);
                            }
                            cursor_ = 0;
                            last_ = data.last != 0;
                            result_ = arg;
                            submit_.set(info);
                            return epoc::error_none;
                        }
                        result = epoc::error_argument;
                    } else {
                        result = epoc::error_argument;
                    }
                }
                info.complete(result);
                return epoc::error_none;
            }
        };

        class ext_sound_factory final : public factory {
        public:
            using factory::factory;
            void install() override {
                obj_name = "ext_sound_drv";
            }
            std::unique_ptr<channel> make_channel(epoc::version version) override {
                return std::make_unique<ext_sound_channel>(kern, sys_, version);
            }
        };
    }

    std::unique_ptr<factory> make_ext_sound_factory(system *system) {
        if (!system->is_uiq_3_device_active()) {
            return nullptr;
        }
        return std::make_unique<ext_sound_factory>(system->get_kernel_system(), system);
    }
}
