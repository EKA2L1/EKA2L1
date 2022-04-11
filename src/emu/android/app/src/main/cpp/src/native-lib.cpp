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
#include <common/android/storage.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>

#include <common/android/jniutils.h>

#if EKA2L1_ARCH(ARM)
#include <cpu/12l1r/tests/test_entry.h>
#endif

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_ANDROID_LOGWRITE

#include <catch2/catch.hpp>

ANativeWindow *s_surf;
std::unique_ptr<eka2l1::android::emulator> state;
bool inited = false;

extern "C" jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    eka2l1::common::jni::virtual_machine = vm;

#if EKA2L1_ARCH(ARM)
    eka2l1::arm::r12l1::register_all_tests();
#endif

    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setDirectory(
    JNIEnv *env,
    jclass clazz,
    jstring path) {
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    const auto executable_directory = eka2l1::file_directory(cpath);
    eka2l1::common::set_current_directory(executable_directory);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_startNative(
    JNIEnv *env,
    jclass clazz) {
    eka2l1::common::jni::init_classloader();
    eka2l1::common::android::register_storage_callbacks(env);

    state = std::make_unique<eka2l1::android::emulator>();
    return emulator_entry(*state);
}

extern "C" JNIEXPORT jobjectArray JNICALL
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

static void redraw_screens_immediately() {
    state->graphics_driver->wait_for(&state->present_status);

    eka2l1::drivers::graphics_command_builder builder;
    state->launcher->draw(builder, state->winserv ? state->winserv->get_screens() : nullptr,
                          state->window->window_fb_size().x,
                          state->window->window_fb_size().y);

    state->present_status = -100;
    builder.present(&state->present_status);

    eka2l1::drivers::command_list retrieved = builder.retrieve_command_list();
    state->graphics_driver->submit_command_list(retrieved);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_launchApp(JNIEnv *env, jclass clazz, jint uid) {
    // Launch the real app...
    state->launcher->launch_app(uid);
}

extern "C" JNIEXPORT void JNICALL
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
Java_com_github_eka2l1_emu_Emulator_surfaceRedrawNeeded(JNIEnv *env, jclass clazz) {
    redraw_screens_immediately();
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_surfaceDestroyed(JNIEnv *env, jclass clazz) {
    pause_threads(*state);
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
    state->window->surface_changed(s_surf, 0, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_pressKey(JNIEnv *env, jclass clazz, jint key,
    jint keyState) {
    press_key(*state, key, keyState);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_touchScreen(JNIEnv *env, jclass clazz, jint x, jint y,
    jint z, jint action, jint pointer_id) {
    touch_screen(*state, x, y, z, action, pointer_id);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_installApp(JNIEnv *env, jclass clazz, jstring path) {
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    return state->launcher->install_app(cpath);
}

static jobjectArray retrieve_jni_string_array_from_vector(JNIEnv *env, const std::vector<std::string> &strings) {
    jobjectArray jdevices = env->NewObjectArray(static_cast<jsize>(strings.size()),
                                                env->FindClass("java/lang/String"),
                                                nullptr);
    for (jsize i = 0; i < strings.size(); ++i)
        env->SetObjectArrayElement(jdevices, i, env->NewStringUTF(strings[i].c_str()));
    return jdevices;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getDevices(
    JNIEnv *env,
    jclass clazz) {
    return retrieve_jni_string_array_from_vector(env, state->launcher->get_devices());
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getDeviceFirmwareCodes(JNIEnv *env, jclass clazz) {
    return retrieve_jni_string_array_from_vector(env, state->launcher->get_device_firwmare_codes());
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setCurrentDevice(JNIEnv *env, jclass clazz, jint id, jboolean is_temp) {
    state->launcher->set_current_device(id, is_temp);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setDeviceName(JNIEnv *env, jclass clazz, jint id, jstring new_name) {
    const char *cstr = env->GetStringUTFChars(new_name, nullptr);
    state->launcher->set_device_name(id, cstr);
    env->ReleaseStringUTFChars(new_name, cstr);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_getCurrentDevice(JNIEnv *env, jclass clazz) {
    return state->launcher->get_current_device();
}

extern "C" JNIEXPORT jint JNICALL
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

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_doesRomNeedRPKG(JNIEnv *env, jclass clazz, jstring rom_path) {
    const char *cstr = env->GetStringUTFChars(rom_path, nullptr);
    const bool result = state->launcher->does_rom_need_rpkg(cstr);

    env->ReleaseStringUTFChars(rom_path, cstr);
    return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
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

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_uninstallPackage(JNIEnv *env, jclass clazz, jint uid, jint ext_index) {
    state->launcher->uninstall_package(uid, ext_index);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_mountSdCard(JNIEnv *env, jclass clazz, jstring path) {
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    state->launcher->mount_sd_card(cpath);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_loadConfig(JNIEnv *env, jclass clazz) {
    state->launcher->load_config();
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setLanguage(JNIEnv *env, jclass clazz, jint language_id) {
    state->launcher->set_language(language_id);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setRtosLevel(JNIEnv *env, jclass clazz, jint level) {
    state->launcher->set_rtos_level(level);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_updateAppSetting(JNIEnv *env, jclass clazz, jint uid) {
    state->launcher->update_app_setting(uid);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getAppIcon(JNIEnv *env, jclass clazz, jlong uid) {
    jobjectArray jicons = state->launcher->get_app_icon(env, uid);
    return jicons;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getLanguageIds(
    JNIEnv *env,
    jclass clazz) {
    std::vector<std::string> language_ids = state->launcher->get_language_ids();
    jobjectArray jlanguage_ids = env->NewObjectArray(static_cast<jsize>(language_ids.size()),
        env->FindClass("java/lang/String"),
        nullptr);
    for (jsize i = 0; i < language_ids.size(); ++i)
        env->SetObjectArrayElement(jlanguage_ids, i, env->NewStringUTF(language_ids[i].c_str()));
    return jlanguage_ids;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getLanguageNames(
    JNIEnv *env,
    jclass clazz) {
    std::vector<std::string> language_names = state->launcher->get_language_names();
    jobjectArray jlanguage_names = env->NewObjectArray(static_cast<jsize>(language_names.size()),
        env->FindClass("java/lang/String"),
        nullptr);
    for (jsize i = 0; i < language_names.size(); ++i)
        env->SetObjectArrayElement(jlanguage_names, i, env->NewStringUTF(language_names[i].c_str()));
    return jlanguage_names;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setScreenParams(JNIEnv *env, jclass clazz,
                                                    jint background_color, jint scale_ratio,
                                                    jint scale_type, jint gravity) {
    state->launcher->set_screen_params(background_color, scale_ratio, scale_type, gravity);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_runTest(JNIEnv *env, jclass clazz, jstring test_name) {
    const char *test_name_c = env->GetStringUTFChars(test_name, nullptr);

    const char *arguments[] = {
            "fake.exe",
            test_name_c
    };

    const int argument_count = 2;

    bool result = (Catch::Session().run(argument_count, arguments) == 0);
    env->ReleaseStringUTFChars(test_name, test_name_c);

    return result;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_submitInput(JNIEnv *env, jclass clazz, jstring text) {
    const char *cstr = env->GetStringUTFChars(text, nullptr);
    std::string ctext = std::string(cstr);
    state->launcher->on_finished_text_input(ctext, false);
}