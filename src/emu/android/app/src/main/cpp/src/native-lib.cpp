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

#include <android/state.h>
#include <android/thread.h>
#include <common/path.h>
#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>

ANativeWindow* s_surf;
std::unique_ptr<eka2l1::android::emulator> state;
bool inited;

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
JNIEXPORT jboolean JNICALL
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
