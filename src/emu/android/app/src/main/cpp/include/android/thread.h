/*
 * Copyright (c) 2019 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project
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

#include <android/state.h>

namespace eka2l1::android {
    struct emulator;

    void graphics_driver_thread(emulator &state);

    void ui_thread(emulator &state);

    void os_thread(emulator &state);

    bool emulator_entry(emulator &state);

    void init_threads(emulator &state);

    void start_threads(emulator &state);

    void pause_threads(emulator &state);

    void press_key(emulator &state, int key, int key_state);

    void touch_screen(emulator &state, int x, int y, int z, int action, int pointer_id);
}