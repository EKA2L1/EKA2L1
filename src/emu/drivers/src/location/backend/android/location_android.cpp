/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <common/android/jniutils.h>
#include <drivers/location/backend/android/location_android.h>

#include <jni.h>
#include <mutex>

namespace eka2l1::drivers {
    static std::mutex host_location_lock;
    static location_update_callback host_location_callback;

    static void call_emulator_location(const char *method) {
        JNIEnv *env = common::jni::environment();
        jclass clazz = common::jni::find_class("com/github/eka2l1/emu/EmulatorLocation");
        jmethodID method_id = env->GetStaticMethodID(clazz, method, "()V");

        env->CallStaticVoidMethod(clazz, method_id);
    }

    location_driver_android::~location_driver_android() {
        stop();
    }

    void location_driver_android::start(location_update_callback callback) {
        {
            const std::lock_guard<std::mutex> guard(host_location_lock);
            host_location_callback = std::move(callback);
        }

        call_emulator_location("start");
    }

    void location_driver_android::stop() {
        {
            const std::lock_guard<std::mutex> guard(host_location_lock);
            host_location_callback = nullptr;
        }

        call_emulator_location("stop");
    }

    namespace android {
        void deliver_host_location(const location_fix &fix) {
            const std::lock_guard<std::mutex> guard(host_location_lock);
            if (host_location_callback) {
                host_location_callback(fix);
            }
        }
    }
}
