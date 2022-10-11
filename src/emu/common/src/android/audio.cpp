/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <common/android/audio.h>
#include <common/android/jniutils.h>

namespace eka2l1::common::android {
    void prepare_audio_record() {
        JNIEnv *env = common::jni::environment();
        jclass clazz = common::jni::find_class("com/github/eka2l1/emu/Emulator");
        jmethodID prepare_audio_record_method = env->GetStaticMethodID(clazz, "prepareAudioRecord", "()V");

        env->CallStaticVoidMethod(clazz, prepare_audio_record_method);
    }
}