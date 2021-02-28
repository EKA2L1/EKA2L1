/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <common/jniutils.h>
#include <drivers/hwrm/backend/vibration_jdk.h>
#include <jni.h>

namespace eka2l1::drivers::hwrm {
    void vibrator_jdk::vibrate(const std::uint32_t millisecs, const std::int16_t intensity) {
        JNIEnv *env = common::jni::environment();
        jclass clazz = common::jni::find_class("com/github/eka2l1/emu/EmulatorActivity");
        jmethodID vibrate_method = env->GetStaticMethodID(clazz, "vibrate", "(I)Z");
        jboolean result = env->CallStaticBooleanMethod(clazz, vibrate_method, (jint) millisecs);
    }
}
