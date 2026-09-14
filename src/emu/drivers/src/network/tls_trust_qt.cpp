// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tls_trust.h"
#include <QSslCertificate>
#include <QSslError>
#include <QSslSocket>

namespace eka2l1::drivers {
    bool verify_system_certificate(const tls_certificate_chain &chain, const std::string &hostname) {
        if (chain.empty()) return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
        if (!QSslSocket::isFeatureSupported(QSsl::SupportedFeature::CertificateVerification)) return false;
#else
        if (!QSslSocket::supportsSsl()) return false;
#endif
        QList<QSslCertificate> certificates;
        for (const auto &der : chain) {
            QSslCertificate certificate(QByteArray(reinterpret_cast<const char *>(der.data()), der.size()), QSsl::Der);
            if (certificate.isNull()) return false;
            certificates.append(certificate);
        }
        return QSslCertificate::verify(certificates, QString::fromStdString(hostname)).empty();
    }
}
