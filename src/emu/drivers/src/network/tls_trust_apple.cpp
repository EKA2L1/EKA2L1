// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tls_trust.h"
#include <Security/Security.h>

namespace eka2l1::drivers {
    bool verify_system_certificate(const tls_certificate_chain &chain, const std::string &hostname) {
        if (chain.empty()) return false;
        auto certificates = CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks);
        if (!certificates) return false;
        bool valid = true;
        for (const auto &der : chain) {
            auto data = CFDataCreate(nullptr, der.data(), der.size());
            auto certificate = data ? SecCertificateCreateWithData(nullptr, data) : nullptr;
            if (data) CFRelease(data);
            if (!certificate) {
                valid = false;
                break;
            }
            CFArrayAppendValue(certificates, certificate);
            CFRelease(certificate);
        }
        auto name = CFStringCreateWithCString(nullptr, hostname.c_str(), kCFStringEncodingUTF8);
        auto policy = name ? SecPolicyCreateSSL(true, name) : nullptr;
        SecTrustRef trust = nullptr;
        valid = valid && policy && SecTrustCreateWithCertificates(certificates, policy, &trust) == errSecSuccess;
        // Trust evaluation runs on the emulator thread; never fetch missing intermediates or revocation data.
        valid = valid && SecTrustSetNetworkFetchAllowed(trust, false) == errSecSuccess
            && SecTrustEvaluateWithError(trust, nullptr);
        if (trust) CFRelease(trust);
        if (policy) CFRelease(policy);
        if (name) CFRelease(name);
        CFRelease(certificates);
        return valid;
    }
}
