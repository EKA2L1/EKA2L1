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

#include <kernel/ldd.h>

#include <memory>

namespace eka2l1 {
    class ntimer;
}

namespace eka2l1::ldd {
    // The "accelerometer" LDD found on S60v3 FP1 devices such as the Nokia N95.
    // RRSensorServer drives it. While started, the driver publishes each sample
    // as three signed bytes (x, y, z) into a Publish & Subscribe property, which
    // rrsensorplugina subscribes to.
    enum accelerometer_opcode {
        ACCELEROMETER_CONTROL_STOP = 0,
        ACCELEROMETER_CONTROL_START = 6,
        ACCELEROMETER_REQUEST_CONFIGURE_SENSOR = 0,
        ACCELEROMETER_REQUEST_CONFIGURE_FILTER = 7,
        ACCELEROMETER_REQUEST_POWER_DOWN = 9
    };

    static constexpr std::uint32_t ACCELEROMETER_DATA_CATEGORY = 0x20000581;
    static constexpr std::uint32_t ACCELEROMETER_DATA_KEY = 0;

    // Publishes samples periodically while at least one channel is started.
    class accelerometer_stream : public std::enable_shared_from_this<accelerometer_stream> {
        kernel_system *kern_;
        system *sys_;
        ntimer *timing_;

        int sample_evt_;
        std::uint32_t listener_count_;

        void publish_sample();
        void schedule_next_sample();
        void on_sample_due();

    public:
        explicit accelerometer_stream(kernel_system *kern, system *sys);
        ~accelerometer_stream();

        void init();
        void add_listener();
        void remove_listener();
    };

    class accelerometer_channel : public channel {
        std::shared_ptr<accelerometer_stream> stream_;
        bool started_;

        void stop();

    public:
        explicit accelerometer_channel(kernel_system *kern, system *sys, epoc::version ver,
            std::shared_ptr<accelerometer_stream> stream);
        ~accelerometer_channel() override;

        std::int32_t do_control(kernel::thread *r, const std::uint32_t n, const eka2l1::ptr<void> arg1,
            const eka2l1::ptr<void> arg2) override;

        std::int32_t do_request(epoc::notify_info info, const std::uint32_t n,
            const eka2l1::ptr<void> arg1, const eka2l1::ptr<void> arg2,
            const bool is_supervisor) override;
    };

    class accelerometer_factory : public factory {
        std::shared_ptr<accelerometer_stream> stream_;

    public:
        explicit accelerometer_factory(kernel_system *kern, system *sys);
        ~accelerometer_factory() override {}

        void install() override;
        std::unique_ptr<channel> make_channel(epoc::version ver) override;
    };
}
