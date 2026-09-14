// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tls_trust.h"
#include <windows.h>
#include <wincrypt.h>

namespace eka2l1::drivers {
    bool verify_system_certificate(const tls_certificate_chain &chain, const std::string &hostname) {
        if (chain.empty()) return false;
        auto store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
        if (!store) return false;
        PCCERT_CONTEXT leaf = nullptr;
        bool valid = true;
        for (std::size_t i = 0; i < chain.size(); ++i) {
            const auto &der = chain[i];
            if (!CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, der.data(),
                static_cast<DWORD>(der.size()), CERT_STORE_ADD_ALWAYS, i == 0 ? &leaf : nullptr)) {
                valid = false;
                break;
            }
        }
        char server_auth[] = szOID_PKIX_KP_SERVER_AUTH;
        char *usages[] = {server_auth};
        CERT_CHAIN_PARA parameters{};
        parameters.cbSize = sizeof(parameters);
        parameters.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
        parameters.RequestedUsage.Usage = {1, usages};
        PCCERT_CHAIN_CONTEXT context = nullptr;
        valid = valid && CertGetCertificateChain(nullptr, leaf, nullptr, store, &parameters,
            CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL, nullptr, &context);
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, hostname.c_str(), -1, nullptr, 0);
        std::wstring name(count, L'\0');
        valid = valid && count > 0 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            hostname.c_str(), -1, name.data(), count) > 0;
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl{};
        ssl.cbSize = sizeof(ssl);
        ssl.dwAuthType = AUTHTYPE_SERVER;
        ssl.pwszServerName = name.data();
        CERT_CHAIN_POLICY_PARA policy{};
        policy.cbSize = sizeof(policy);
        policy.pvExtraPolicyPara = &ssl;
        CERT_CHAIN_POLICY_STATUS status{};
        status.cbSize = sizeof(status);
        valid = valid && CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, context, &policy, &status)
            && status.dwError == 0;
        if (context) CertFreeCertificateChain(context);
        if (leaf) CertFreeCertificateContext(leaf);
        CertCloseStore(store, 0);
        return valid;
    }
}
