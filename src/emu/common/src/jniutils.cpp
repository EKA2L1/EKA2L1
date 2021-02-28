/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <common/jniutils.h>
#include <common/log.h>

namespace eka2l1::common::jni {
    JavaVM *virtual_machine = nullptr;

    jobject c_classloader;
    jmethodID c_findclass_method;

    void init_classloader() {
        JNIEnv *env = environment();
        jclass clazz = env->FindClass("com/github/eka2l1/emu/EmulatorActivity");
        jclass classloader_class = env->FindClass("java/lang/ClassLoader");
        jmethodID get_classloader_method = env->GetStaticMethodID(clazz, "getAppClassLoader", "()Ljava/lang/ClassLoader;");
        c_classloader = env->NewGlobalRef(env->CallStaticObjectMethod(clazz, get_classloader_method));
        c_findclass_method = env->GetMethodID(classloader_class, "findClass",
                                            "(Ljava/lang/String;)Ljava/lang/Class;");
    }

    jclass find_class(const char* name) {
        JNIEnv *env = environment();
        return static_cast<jclass>(env->CallObjectMethod(c_classloader, c_findclass_method, env->NewStringUTF(name)));
    }

    JNIEnv *environment() {
        JNIEnv *env = nullptr;

        if (!virtual_machine) {
            LOG_ERROR(COMMON, "Virtual machine not available!");
            return nullptr;
        }

        const int env_status = virtual_machine->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

        if (env_status == JNI_EDETACHED) {
            if (virtual_machine->AttachCurrentThread(&env, nullptr) != 0) {
                LOG_ERROR(COMMON, "Unable to attach environment to current thread");
                return nullptr;
            }
        }

        return env;
    }
}