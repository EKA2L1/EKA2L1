/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include "emulator_camera_jni.h"
#include <common/android/jniutils.h>
#include <jni.h>

namespace eka2l1::drivers::android {
    static jclass camera_class = nullptr;
    static jmethodID camera_count_method = nullptr;
    static jmethodID camera_initialize_method = nullptr;
    static jmethodID camera_is_facing_front_method = nullptr;
    static jmethodID camera_supported_output_image_format_method = nullptr;
    static jmethodID camera_get_flash_mode_method = nullptr;
    static jmethodID camera_set_flash_mode_method = nullptr;
    static jmethodID camera_get_output_image_size_method = nullptr;
    static jmethodID camera_capture_image_method = nullptr;
    static jmethodID camera_receive_viewfinder_feed_method = nullptr;
    static jmethodID camera_stop_viewfinder_feed_method = nullptr;

    void register_camera_callbacks(JNIEnv *env) {
        jclass temp_camera_class = env->FindClass("com/github/eka2l1/emu/EmulatorCamera");
        camera_class = (jclass) env->NewGlobalRef(temp_camera_class);
        env->DeleteLocalRef(temp_camera_class);

        camera_count_method = env->GetStaticMethodID(camera_class, "getCameraCount", "()I");
        camera_initialize_method = env->GetStaticMethodID(camera_class, "initializeCamera", "(I)I");
        camera_supported_output_image_format_method = env->GetStaticMethodID(camera_class,
             "getSupportedImageOutputFormats", "()[I");
        camera_get_flash_mode_method = env->GetStaticMethodID(camera_class, "getFlashMode", "(I)I");
        camera_set_flash_mode_method = env->GetStaticMethodID(camera_class, "setFlashMode", "(II)Z");
        camera_is_facing_front_method = env->GetStaticMethodID(camera_class, "isCameraFacingFront", "(I)Z");
        camera_get_output_image_size_method = env->GetStaticMethodID(camera_class, "getOutputImageSizes", "(I)[Landroid/util/Size;");
        camera_capture_image_method = env->GetStaticMethodID(camera_class, "captureImage", "(III)Z");
        camera_receive_viewfinder_feed_method = env->GetStaticMethodID(camera_class, "receiveViewfinderFeed", "(IIII)Z");
        camera_stop_viewfinder_feed_method = env->GetStaticMethodID(camera_class, "stopViewfinderFeed", "(I)V");
    }

    int emulator_camera_count() {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticIntMethod(camera_class, camera_count_method);
    }

    int emulator_camera_initialize(int index) {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticIntMethod(camera_class, camera_initialize_method, index);
    }

    bool emulator_camera_is_facing_front(int handle) {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticBooleanMethod(camera_class, camera_is_facing_front_method, handle);
    }

    std::vector<int> emulator_camera_get_supported_image_output_formats() {
        JNIEnv *env = common::jni::environment();
        jintArray support_array = reinterpret_cast<jintArray>(env->CallStaticObjectMethod(
                camera_class, camera_supported_output_image_format_method));

        std::vector<int> support_list_native(env->GetArrayLength(support_array));
        env->GetIntArrayRegion(support_array, 0, static_cast<jsize>(support_list_native.size()), &support_list_native[0]);

        return support_list_native;
    }

    int emulator_camera_get_flash_mode(int handle) {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticIntMethod(camera_class, camera_get_flash_mode_method, handle);
    }

    bool emulator_camera_set_flash_mode(int handle, int mode) {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticBooleanMethod(camera_class, camera_set_flash_mode_method, handle, mode);
    }

    std::vector<eka2l1::vec2> emulator_camera_get_output_image_sizes(int handle) {
        std::vector<eka2l1::vec2> result;
        JNIEnv *env = common::jni::environment();

        jobjectArray size_array = static_cast<jobjectArray>(env->CallStaticObjectMethod(
                camera_class, camera_get_output_image_size_method, handle));

        result.resize(env->GetArrayLength(size_array));

        jclass size_class = env->FindClass("android/util/Size");

        jmethodID size_get_width_method = env->GetMethodID(size_class, "getWidth", "()I");
        jmethodID size_get_height_method = env->GetMethodID(size_class, "getHeight", "()I");

        for (std::size_t i = 0; i < result.size(); i++) {
            jobject obj = env->GetObjectArrayElement(size_array, i);

            result[i].x = env->CallIntMethod(obj, size_get_width_method);
            result[i].y = env->CallIntMethod(obj, size_get_height_method);
        }

        return result;
    }

    bool emulator_camera_capture_image(int handle, int resolution_index, int format) {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticBooleanMethod(camera_class, camera_capture_image_method, handle, resolution_index, format);
    }

    bool emulator_camera_receive_viewfinder_feed(int handle, int width, int height, int format) {
        JNIEnv *env = common::jni::environment();
        return env->CallStaticBooleanMethod(camera_class, camera_receive_viewfinder_feed_method, handle, width, height, format);
    }

    void emulator_camera_stop_viewfinder_feed(int handle) {
        JNIEnv *env = common::jni::environment();
        env->CallStaticVoidMethod(camera_class, camera_stop_viewfinder_feed_method, handle);

    }
}