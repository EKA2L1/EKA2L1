/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <drivers/hwrm/vibration.h>

namespace eka2l1::drivers::hwrm {
    void set_controller_haptic_source(void *controller);
    void set_vibration_suspended(bool suspended);

    class vibrator_ios final : public vibrator {
    public:
        vibrator_ios();
        ~vibrator_ios() override;

        void vibrate(const std::uint32_t millisecs, const std::int16_t intensity = 0) override;
        void stop_vibrate() override;

    private:
        void *engine_ = nullptr;
        void *player_ = nullptr;
        std::uint64_t source_revision_ = 0;
        void clear_engine_locked();
        void stop_player_locked();
    };
}
