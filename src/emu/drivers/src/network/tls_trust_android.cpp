// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tls_trust.h"
#include <common/android/jniutils.h>

namespace eka2l1::drivers {
    bool verify_system_certificate(const tls_certificate_chain &chain, const std::string &hostname) {
        auto *env = common::jni::environment();
        if (!env || chain.empty()) return false;
        if (env->PushLocalFrame(16) < 0) {
            env->ExceptionClear();
            return false;
        }
        bool valid = false;
        auto clazz = common::jni::find_class("com/github/eka2l1/emu/TlsTrust");
        auto verify = clazz ? env->GetStaticMethodID(clazz, "verify", "([[BLjava/lang/String;)Z") : nullptr;
        auto bytes = verify ? env->FindClass("[B") : nullptr;
        auto certificates = bytes ? env->NewObjectArray(chain.size(), bytes, nullptr) : nullptr;
        auto name = certificates ? env->NewStringUTF(hostname.c_str()) : nullptr;
        if (name) {
            for (std::size_t i = 0; i < chain.size(); ++i) {
                auto der = env->NewByteArray(chain[i].size());
                if (!der) break;
                env->SetByteArrayRegion(der, 0, chain[i].size(), reinterpret_cast<const jbyte *>(chain[i].data()));
                env->SetObjectArrayElement(certificates, i, der);
                env->DeleteLocalRef(der);
                if (env->ExceptionCheck()) break;
            }
            if (!env->ExceptionCheck()) valid = env->CallStaticBooleanMethod(clazz, verify, certificates, name);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            valid = false;
        }
        env->PopLocalFrame(nullptr);
        return valid;
    }
}
