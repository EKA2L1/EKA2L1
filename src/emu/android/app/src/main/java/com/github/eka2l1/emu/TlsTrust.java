// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later
package com.github.eka2l1.emu;

import android.net.http.X509TrustManagerExtensions;

import java.io.ByteArrayInputStream;
import java.security.KeyStore;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

public final class TlsTrust {
    private TlsTrust() {}

    public static boolean verify(byte[][] encodedChain, String hostname) {
        try {
            if (encodedChain.length == 0) return false;
            CertificateFactory certificates = CertificateFactory.getInstance("X.509");
            X509Certificate[] chain = new X509Certificate[encodedChain.length];
            for (int i = 0; i < chain.length; ++i) {
                chain[i] = (X509Certificate) certificates.generateCertificate(new ByteArrayInputStream(encodedChain[i]));
            }
            TrustManagerFactory factory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
            factory.init((KeyStore) null);
            for (TrustManager manager : factory.getTrustManagers()) {
                if (manager instanceof X509TrustManager) {
                    // The native TLS layer checks the leaf's hostname before calling the system chain verifier.
                    new X509TrustManagerExtensions((X509TrustManager) manager)
                            .checkServerTrusted(chain, chain[0].getPublicKey().getAlgorithm(), hostname);
                    return true;
                }
            }
        } catch (Exception exception) {
            return false;
        }
        return false;
    }
}
