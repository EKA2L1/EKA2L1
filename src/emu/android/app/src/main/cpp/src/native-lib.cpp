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
#include <filesystem>

#include <android/state.h>
#include <android/input_dialog.h>
#include <android/thread.h>
#include <common/android/storage.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <config/config.h>
#include <drivers/audio/audio.h>
#include <drivers/camera/backend/android/emulator_camera_jni_public.h>
#include <drivers/camera/camera_collection.h>
#include <drivers/camera/backend/android/camera_collection_android.h>
#include <drivers/graphics/graphics.h>
#include <drivers/location/backend/android/location_android.h>

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

static std::unique_lock<std::recursive_mutex> lock_debug_control() {
    if (state->debug_backend) return std::unique_lock<std::recursive_mutex>(state->debug_mutex);
    return {};
}

static void debug_java_call(const char *method, const char *signature, jlong uid = 0) {
    JNIEnv *env = eka2l1::common::jni::environment();
    jclass clazz = eka2l1::common::jni::find_class("com/github/eka2l1/emu/Emulator");
    jmethodID id = env->GetStaticMethodID(clazz, method, signature);
    if (signature[1] == 'J') env->CallStaticVoidMethod(clazz, id, uid);
    else env->CallStaticVoidMethod(clazz, id);
    env->DeleteLocalRef(clazz);
}

static void start_debug_control(int port) {
    if (port <= 0 || port > 65535) return;
    using namespace eka2l1;
    control::native_frontend frontend;
    frontend.platform = "android";
    frontend.documents = std::filesystem::current_path().string();
    frontend.log = (std::filesystem::current_path() / "EKA2L1.log").string();
    frontend.get_system = [] { return state->symsys.get(); };
    frontend.frames = [] { return state->debug_frames.load(); };
    frontend.can_capture = [] { return s_surf && !state->should_graphics_pause.load(); };
    frontend.launch = [](std::uint32_t uid) { debug_java_call("debugLaunchApp", "(J)V", uid); return true; };
    frontend.close = [] {
        state->debug_backend->reset_session();
        android::prepare_control_reset(*state);
        state->symsys->reset();
        debug_java_call("debugCloseApp", "()V");
    };
    frontend.boot = [](int index) { debug_java_call("debugCloseApp", "()V"); return android::control_boot_device(*state,index); };
    frontend.pause = [](bool paused) {
        state->should_emu_pause = paused;
        if (!paused) state->pause_sema.notify();
    };
    frontend.changed = [](control::native_change) { debug_java_call("debugContentChanged", "()V"); };
    if (state->sensor_driver) {
        state->sensor_driver->set_acceleration_override(nullptr);
        frontend.acceleration = [](const double *values) { state->sensor_driver->set_acceleration_override(values); };
    }
    state->debug_backend = std::make_shared<control::native_backend>(std::move(frontend));
    state->debug_service = std::make_unique<control::service>([](auto req, auto done) {
        std::lock_guard<std::recursive_mutex> guard(state->debug_mutex);
        done(state->debug_backend->handle(req));
    });
    state->debug_server = std::make_unique<control::server>();
    std::string error;
    if (!state->debug_server->start(port, [](auto req, auto done) { state->debug_service->handle(std::move(req),std::move(done)); },error)) {
        LOG_ERROR(FRONTEND_CMDLINE,"Unable to start debug control: {}",error);
    } else {
        LOG_INFO(FRONTEND_CMDLINE,"Debug control on 127.0.0.1:{}",port);
    }
}

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
    jclass clazz, jint debug_port) {
    eka2l1::common::jni::init_classloader();
    eka2l1::common::android::register_storage_callbacks(env);
    eka2l1::drivers::android::register_camera_callbacks(env);

    state = std::make_unique<eka2l1::android::emulator>();
    state->debug_control_enabled = debug_port > 0 && debug_port <= 65535;
    const bool ready = emulator_entry(*state);
    start_debug_control(debug_port);
    return ready || (debug_port > 0 && debug_port <= 65535);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getApps(
    JNIEnv *env,
    jclass clazz) {
    auto control_guard = lock_debug_control();
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
    auto control_guard = lock_debug_control();
    if (state->debug_backend) {
        state->debug_backend->launch_app(uid, [](eka2l1::kernel::process *) { debug_java_call("debugCloseApp", "()V"); });
    } else state->launcher->launch_app(uid);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_surfaceChanged(JNIEnv *env, jclass clazz, jobject surface,
    jint width, jint height) {
    auto control_guard = lock_debug_control();
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
    auto control_guard = lock_debug_control();
    pause_threads(*state);
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
    state->window->surface_changed(s_surf, 0, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_pressKey(JNIEnv *env, jclass clazz, jint key,
    jint keyState) {
    auto control_guard = lock_debug_control();
    press_key(*state, key, keyState);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_touchScreen(JNIEnv *env, jclass clazz, jint x, jint y,
    jint z, jint action, jint pointer_id) {
    auto control_guard = lock_debug_control();
    touch_screen(*state, x, y, z, action, pointer_id);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_installApp(JNIEnv *env, jclass clazz, jstring path) {
    auto control_guard = lock_debug_control();
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
    auto control_guard = lock_debug_control();
    return retrieve_jni_string_array_from_vector(env, state->launcher->get_devices());
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getDeviceFirmwareCodes(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    return retrieve_jni_string_array_from_vector(env, state->launcher->get_device_firwmare_codes());
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setCurrentDevice(JNIEnv *env, jclass clazz, jint id, jboolean is_temp) {
    auto control_guard = lock_debug_control();
    state->launcher->set_current_device(id, is_temp);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setDeviceName(JNIEnv *env, jclass clazz, jint id, jstring new_name) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(new_name, nullptr);
    state->launcher->set_device_name(id, cstr);
    env->ReleaseStringUTFChars(new_name, cstr);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_deleteDevice(JNIEnv *env, jclass clazz, jint id) {
    auto control_guard = lock_debug_control();
    return state->launcher->delete_device(id);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_rescanDevices(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    state->launcher->rescan_devices();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_getCurrentDevice(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    return state->launcher->get_current_device();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_isTouchScreenDevice(JNIEnv *env, jclass clazz, jstring firmware_code) {
    auto control_guard = lock_debug_control();
    std::string code;
    if (firmware_code) {
        const char *cstr = env->GetStringUTFChars(firmware_code, nullptr);
        code = cstr;
        env->ReleaseStringUTFChars(firmware_code, cstr);
    }
    return state->launcher->is_touch_screen_device(code);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_installDevice(JNIEnv *env, jclass clazz, jstring rpkg_path,
    jstring rom_path, jboolean install_rpkg, jboolean isolate_drives) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(rpkg_path, nullptr);
    std::string crpkg_path = std::string(cstr);
    env->ReleaseStringUTFChars(rpkg_path, cstr);
    cstr = env->GetStringUTFChars(rom_path, nullptr);
    std::string crom_path = std::string(cstr);
    env->ReleaseStringUTFChars(rom_path, cstr);

    return state->launcher->install_device(crpkg_path, crom_path, install_rpkg, isolate_drives);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_doesRomNeedRPKG(JNIEnv *env, jclass clazz, jstring rom_path) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(rom_path, nullptr);
    const bool result = state->launcher->does_rom_need_rpkg(cstr);

    env->ReleaseStringUTFChars(rom_path, cstr);
    return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getPackages(
    JNIEnv *env,
    jclass clazz) {
    auto control_guard = lock_debug_control();
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
    auto control_guard = lock_debug_control();
    state->launcher->uninstall_package(uid, ext_index);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_mountSdCard(JNIEnv *env, jclass clazz, jstring path) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    state->launcher->mount_sd_card(cpath);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_loadConfig(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    state->launcher->load_config();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_validHostMapping(JNIEnv *env, jclass clazz, jstring hostname, jstring target) {
    if (!hostname || !target) {
        return JNI_FALSE;
    }
    const char *name = env->GetStringUTFChars(hostname, nullptr);
    if (!name) {
        return JNI_FALSE;
    }
    const std::string name_copy(name);
    env->ReleaseStringUTFChars(hostname, name);
    const char *address = env->GetStringUTFChars(target, nullptr);
    if (!address) {
        return JNI_FALSE;
    }
    const bool valid = eka2l1::config::valid_host_pattern(name_copy) && eka2l1::config::valid_host_target(address);
    env->ReleaseStringUTFChars(target, address);
    return valid;
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setLanguage(JNIEnv *env, jclass clazz, jint language_id) {
    auto control_guard = lock_debug_control();
    state->launcher->set_language(language_id);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setRtosLevel(JNIEnv *env, jclass clazz, jint level) {
    auto control_guard = lock_debug_control();
    state->launcher->set_rtos_level(level);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_updateAppSetting(JNIEnv *env, jclass clazz, jint uid) {
    auto control_guard = lock_debug_control();
    state->launcher->update_app_setting(uid);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getScreenModeSizes(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    std::vector<eka2l1::vec2> sizes = state->launcher->get_screen_mode_sizes();
    std::vector<jint> flat;
    for (const eka2l1::vec2 &size : sizes) {
        flat.push_back(size.x);
        flat.push_back(size.y);
    }
    jintArray result = env->NewIntArray(static_cast<jsize>(flat.size()));
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(flat.size()), flat.data());
    return result;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_github_eka2l1_emu_Emulator_getAppIcon(JNIEnv *env, jclass clazz, jlong uid) {
    auto control_guard = lock_debug_control();
    return state->launcher->get_app_icon(env, uid);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getLanguageIds(
    JNIEnv *env,
    jclass clazz) {
    auto control_guard = lock_debug_control();
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
    auto control_guard = lock_debug_control();
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
                                                    jint scale_type, jint gravity,
                                                    jstring bg_img_path, jfloat bg_img_opacity,
                                                    jboolean bg_img_keep_aspect) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(bg_img_path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(bg_img_path, cstr);

    state->launcher->set_screen_params(background_color, scale_ratio, scale_type, gravity, cpath, bg_img_opacity, bg_img_keep_aspect);
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
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(text, nullptr);
    std::string ctext = std::string(cstr);
    state->launcher->on_finished_text_input(ctext, false);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_EmulatorCamera_onCaptureImageDelivered(JNIEnv *env, jclass clazz,
                                                                  jint index, jbyteArray raw_data,
                                                                  jint error_code) {
    eka2l1::drivers::camera::collection_android *collection = reinterpret_cast
            <eka2l1::drivers::camera::collection_android *>(eka2l1::drivers::camera::get_collection());

    if (collection) {
        jboolean is_data_copy = false;
        jsize data_size = env->GetArrayLength(raw_data);
        jbyte *data = env->GetByteArrayElements(raw_data, &is_data_copy);
        collection->handle_image_capture_delivered(index, data, static_cast<int>(data_size), error_code);
        env->ReleaseByteArrayElements(raw_data, data, 0);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_EmulatorCamera_onFrameViewfinderDelivered(JNIEnv *env, jclass clazz,
                                                                     jint index,
                                                                     jbyteArray raw_data,
                                                                     jint error_code) {
    eka2l1::drivers::camera::collection_android *collection = reinterpret_cast
            <eka2l1::drivers::camera::collection_android *>(eka2l1::drivers::camera::get_collection());

    if (collection) {
        jboolean is_data_copy = false;
        jsize data_size = env->GetArrayLength(raw_data);
        jbyte *data = env->GetByteArrayElements(raw_data, &is_data_copy);
        collection->handle_frame_viewfinder_delivered(index, data, static_cast<int>(data_size), error_code);
        env->ReleaseByteArrayElements(raw_data, data, 0);
    }
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_EmulatorCamera_guestFrameRotation(JNIEnv *env, jclass clazz) {
    return eka2l1::drivers::camera::frame_rotation();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_EmulatorCamera_doesCameraAllowNewFrame(JNIEnv *env, jclass clazz,
                                                                  jint index) {
    eka2l1::drivers::camera::collection_android *collection = reinterpret_cast
            <eka2l1::drivers::camera::collection_android *>(eka2l1::drivers::camera::get_collection());

    return collection->reserved_wants_new_frame(index);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_github_eka2l1_emu_Emulator_installNGageGame(JNIEnv *env, jclass clazz, jstring path) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(path, nullptr);
    std::string cpath = std::string(cstr);
    env->ReleaseStringUTFChars(path, cstr);

    return state->launcher->install_ngage_game(cpath);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_submitQuestionDialogResponse(JNIEnv *env, jclass clazz,
                                                                 jlong id, jint value) {
    auto control_guard = lock_debug_control();
    state->launcher->on_question_dialog_finished(static_cast<std::uint64_t>(id), value);
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getSuccessInstalledLicenseGames(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    return retrieve_jni_string_array_from_vector(env, state->launcher->get_success_installed_license_games());
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_github_eka2l1_emu_Emulator_getFailedInstalledLicenseGames(JNIEnv *env, jclass clazz) {
    auto control_guard = lock_debug_control();
    return retrieve_jni_string_array_from_vector(env, state->launcher->get_failed_installed_license_games());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_installNG2Licenses(JNIEnv *env, jclass clazz, jstring content) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(content, nullptr);
    std::string content_cpp = std::string(cstr);
    env->ReleaseStringUTFChars(content, cstr);

    return state->launcher->install_ng2_game_licenses(content_cpp);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_setCurrentMMCID(JNIEnv *env, jclass clazz, jstring new_mmcid) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(new_mmcid, nullptr);
    std::string mmc_id_str = std::string(cstr);
    env->ReleaseStringUTFChars(new_mmcid, cstr);

    state->launcher->set_current_mmc_id(mmc_id_str);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_github_eka2l1_emu_Emulator_saveScreenshotTo(JNIEnv *env, jclass clazz, jstring file_path) {
    auto control_guard = lock_debug_control();
    const char *cstr = env->GetStringUTFChars(file_path, nullptr);
    std::string file_path_std = std::string(cstr);
    env->ReleaseStringUTFChars(file_path, cstr);

    return state->launcher->save_screenshot_to(file_path_std);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_EmulatorLocation_onLocationChanged(JNIEnv *env, jclass clazz,
                                                              jdouble latitude, jdouble longitude,
                                                              jfloat altitude,
                                                              jfloat horizontal_accuracy,
                                                              jfloat vertical_accuracy,
                                                              jfloat speed, jfloat speed_accuracy,
                                                              jfloat course, jfloat course_accuracy) {
    eka2l1::drivers::location_fix fix;
    fix.latitude_ = latitude;
    fix.longitude_ = longitude;
    fix.altitude_ = altitude;
    fix.horizontal_accuracy_ = horizontal_accuracy;
    fix.vertical_accuracy_ = vertical_accuracy;
    fix.speed_ = speed;
    fix.speed_accuracy_ = speed_accuracy;
    fix.course_ = course;
    fix.course_accuracy_ = course_accuracy;

    eka2l1::drivers::android::deliver_host_location(fix);
}

extern "C" JNIEXPORT void JNICALL
Java_com_github_eka2l1_emu_Emulator_debugCloseNative(JNIEnv *, jclass) {
    if (!state || !state->debug_backend) return;
    std::lock_guard<std::recursive_mutex> guard(state->debug_mutex);
    state->debug_backend->reset_session();
    eka2l1::android::prepare_control_reset(*state);
    state->symsys->reset();
}
