/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <jni.h>
#include <string>

#include <android/bitmap.h>
#include <android/state.h>
#include <android/thread.h>
#include <common/buffer.h>
#include <common/path.h>
#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>
#include <services/fbs/bitmap.h>

#include <common/jniutils.h>

ANativeWindow* s_surf;
std::unique_ptr<eka2l1::android::emulator> state;
bool inited;

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    eka2l1::common::jni::virtual_machine = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setDirectory(
        JNIEnv* env,
        jclass clazz,
        jstring path) {
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    const auto executable_directory = eka2l1::file_directory(cpath);
    eka2l1::set_current_directory(executable_directory);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_startNative(
        JNIEnv* env,
        jclass clazz) {
    state = std::make_unique<eka2l1::android::emulator>();
    return emulator_entry(*state);
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getApps(
        JNIEnv *env,
        jclass clazz) {
    std::vector<std::string> info = state->launcher->get_apps();
    jobjectArray japps = env->NewObjectArray(static_cast<jsize>(info.size()),
                                              env->FindClass("java/lang/String"),
                                              nullptr);
    for (jsize i = 0; i < info.size(); ++i)
        env->SetObjectArrayElement(japps, i, env->NewStringUTF(info[i].c_str()));
    return japps;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_launchApp(JNIEnv *env, jclass clazz, jint uid) {
    eka2l1::common::jni::init_classloader();
    state->launcher->launch_app(uid);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_surfaceChanged(JNIEnv *env, jclass clazz, jobject surface,
                                                   jint width, jint height) {
    s_surf = ANativeWindow_fromSurface(env, surface);
    state->window->surface_changed(s_surf, width, height);
    if (!inited) {
        init_threads(*state);
        inited = true;
    } else {
        start_threads(*state);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_surfaceDestroyed(JNIEnv *env, jclass clazz) {
    pause_threads(*state);
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
    state->window->surface_changed(s_surf, 0, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_pressKey(JNIEnv *env, jclass clazz, jint key,
                                             jint keyState) {
    press_key(*state, key, keyState);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_touchScreen(JNIEnv *env, jclass clazz, jint x, jint y,
                                                  jint action) {
    touch_screen(*state, x, y, action);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_installApp(JNIEnv *env, jclass clazz, jstring path) {
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    return state->launcher->install_app(cpath);
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getDevices(
        JNIEnv *env,
        jclass clazz) {
    std::vector<std::string> info = state->launcher->get_devices();
    jobjectArray jdevices = env->NewObjectArray(static_cast<jsize>(info.size()),
                                             env->FindClass("java/lang/String"),
                                             nullptr);
    for (jsize i = 0; i < info.size(); ++i)
        env->SetObjectArrayElement(jdevices, i, env->NewStringUTF(info[i].c_str()));
    return jdevices;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setCurrentDevice(JNIEnv *env, jclass clazz, jint id) {
    state->launcher->set_current_device(id);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_getCurrentDevice(JNIEnv *env, jclass clazz) {
    return state->launcher->get_current_device();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_installDevice(JNIEnv *env, jclass clazz, jstring rpkg_path,
                                                jstring rom_path, jboolean install_rpkg) {
    const char *cstr = env->GetStringUTFChars(rpkg_path, nullptr);
    std::string crpkg_path = std::string(cstr);
    env->ReleaseStringUTFChars(rpkg_path, cstr);
    cstr = env->GetStringUTFChars(rom_path, nullptr);
    std::string crom_path = std::string(cstr);
    env->ReleaseStringUTFChars(rom_path, cstr);

    return state->launcher->install_device(crpkg_path, crom_path, install_rpkg);
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getPackages(
        JNIEnv *env,
        jclass clazz) {
    std::vector<std::string> info = state->launcher->get_packages();
    jobjectArray jpackages = env->NewObjectArray(static_cast<jsize>(info.size()),
                                             env->FindClass("java/lang/String"),
                                             nullptr);
    for (jsize i = 0; i < info.size(); ++i)
        env->SetObjectArrayElement(jpackages, i, env->NewStringUTF(info[i].c_str()));
    return jpackages;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_uninstallPackage(JNIEnv *env, jclass clazz, jint uid) {
    state->launcher->uninstall_package(uid);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_mountSdCard(JNIEnv *env, jclass clazz, jstring path) {
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    state->launcher->mount_sd_card(cpath);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_loadConfig(JNIEnv *env, jclass clazz) {
    state->launcher->load_config();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setLanguage(JNIEnv *env, jclass clazz, jint language_id) {
    state->launcher->set_language(language_id);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setRtosLevel(JNIEnv *env, jclass clazz, jint level) {
    state->launcher->set_rtos_level(level);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_updateAppSetting(JNIEnv *env, jclass clazz, jint uid) {
    state->launcher->update_app_setting(uid);
}

static jobject make_new_bitmap(JNIEnv *env, std::uint32_t width, std::uint32_t height) {
    jclass bitmapCls = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapFunction = env->GetStaticMethodID(bitmapCls,
        "createBitmap","(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jstring configName = env->NewStringUTF("ARGB_8888");
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jmethodID valueOfBitmapConfigFunction = env->GetStaticMethodID(
        bitmapConfigClass, "valueOf","(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");

    jobject bitmapConfig = env->CallStaticObjectMethod(bitmapConfigClass, valueOfBitmapConfigFunction,
        configName);

    jobject newBitmap = env->CallStaticObjectMethod(bitmapCls, createBitmapFunction, width,
        height, bitmapConfig);

    return newBitmap;
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getAppIcon(JNIEnv *env, jclass clazz, jlong uid) {
    std::optional<eka2l1::apa_app_masked_icon_bitmap > icon_pair = state->launcher->get_app_icon(uid);
    if (!icon_pair) {
        return nullptr;
    }
    jobjectArray jicons = env->NewObjectArray(icon_pair->second ? 2 : 1,
        env->FindClass("android/graphics/Bitmap"),
        nullptr);

    jobject source_bitmap = make_new_bitmap(env, icon_pair->first->header_.size_pixels.x,
        icon_pair->first->header_.size_pixels.y);

    void *data_to_write = nullptr;
    int result = AndroidBitmap_lockPixels(env, source_bitmap, &data_to_write);
    if (result < 0) {
        env->DeleteLocalRef(source_bitmap);
        env->DeleteLocalRef(jicons);
        return nullptr;
    }

    eka2l1::common::wo_buf_stream dest_stream(reinterpret_cast<std::uint8_t*>(data_to_write), icon_pair->first->header_.size_pixels.x
        * icon_pair->first->header_.size_pixels.y * 4);

    if (!eka2l1::epoc::convert_to_argb8888(state->launcher->get_fbs_serv(), icon_pair->first, dest_stream)) {
        env->DeleteLocalRef(source_bitmap);
        env->DeleteLocalRef(jicons);

        return nullptr;
    }

    AndroidBitmap_unlockPixels(env, source_bitmap);
    env->SetObjectArrayElement(jicons, 0, source_bitmap);

    if (icon_pair->second) {
        jobject mask_bitmap = make_new_bitmap(env, icon_pair->second->header_.size_pixels.x,
            icon_pair->second->header_.size_pixels.y);

        result = AndroidBitmap_lockPixels(env, mask_bitmap, &data_to_write);
        if (result < 0) {
            env->DeleteLocalRef(mask_bitmap);
            return jicons;
        }

        dest_stream = eka2l1::common::wo_buf_stream(reinterpret_cast<std::uint8_t*>(data_to_write),
            icon_pair->second->header_.size_pixels.x * icon_pair->second->header_.size_pixels.y * 4);

        if (!eka2l1::epoc::convert_to_argb8888(state->launcher->get_fbs_serv(), icon_pair->second, dest_stream)) {
            env->DeleteLocalRef(mask_bitmap);
            return jicons;
        }

        AndroidBitmap_unlockPixels(env, mask_bitmap);
        env->SetObjectArrayElement(jicons, 1, mask_bitmap);
    }

    return jicons;
}
