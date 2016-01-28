/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#ifdef _WIN32
// needed for crypto API compatibility - otherwise this will only work on Win8+
#  define WINVER _WIN32_WINNT_WIN7
#  define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#include "exception.h"
#include "cryptography.h"
#include "signature_p.h"

#include <windows.h>
#include <wincrypt.h>


class WinCryptException : public Exception
{
public:
    WinCryptException(const char *errorString)
        : Exception(Error::Cryptography)
    {
        m_errorString = Cryptography::errorString(GetLastError(), errorString);
    }
};

QByteArray SignaturePrivate::create(const QByteArray &signingCertificatePkcs12, const QByteArray &signingCertificatePassword) throw(Exception)
{
    HCERTSTORE certStore = nullptr;

    auto cleanup = [=]() {
        if (certStore)
            CertCloseStore(certStore, CERT_CLOSE_STORE_FORCE_FLAG);
    };

    try {
        // Although WinCrypt could, the OSX Security Framework cannot process empty detached data
        if (hash.isEmpty())
            throw WinCryptException("cannot sign an empty hash value");

        CRYPT_DATA_BLOB pkcs12Blob;
        pkcs12Blob.cbData = signingCertificatePkcs12.size();
        pkcs12Blob.pbData = (BYTE *) signingCertificatePkcs12.constData();

        QString password = QString::fromUtf8(signingCertificatePassword);

        certStore = PFXImportCertStore(&pkcs12Blob,
                                       reinterpret_cast<const wchar_t *>(password.utf16()),
                                       CRYPT_EXPORTABLE);
        if (!certStore)
            throw WinCryptException("could not read or not parse PKCS#12 certificate");

        QVector<PCCERT_CONTEXT> signCerts;
        QVector<PCCERT_CONTEXT> allCerts;

        PCCERT_CONTEXT cert = 0;
        while (cert = CertEnumCertificatesInStore(certStore, cert)) {
            BYTE keyUsage = 0;
            if (!CertGetIntendedKeyUsage(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, cert->pCertInfo,
                                         &keyUsage, sizeof(keyUsage))
                    || !(keyUsage & CERT_KEY_CERT_SIGN_KEY_USAGE)) {
                signCerts << cert;
            }
            allCerts << cert;
        }
        CertFreeCertificateContext(cert);

        if (signCerts.size() != 1)
            throw WinCryptException("PKCS#12 key did not contain exactly 1 signing certificate");

        CRYPT_SIGN_MESSAGE_PARA cmsParams;
        memset(&cmsParams, 0, sizeof(cmsParams));
        cmsParams.cbSize = sizeof(cmsParams);
        cmsParams.dwMsgEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
        cmsParams.pSigningCert = signCerts.first();
        cmsParams.HashAlgorithm.pszObjId = (LPSTR) szOID_OIWSEC_sha1;
        cmsParams.cMsgCert = allCerts.size();
        cmsParams.rgpMsgCert = allCerts.data();
        cmsParams.dwFlags = CRYPT_MESSAGE_SILENT_KEYSET_FLAG;
        const BYTE *inData[] = { (const BYTE *) hash.constData() };
        DWORD inSize[] = { (DWORD) hash.size() };
        DWORD outSize = 0;
        if (!CryptSignMessage(&cmsParams, true, 1, inData, inSize, nullptr, &outSize))
            throw WinCryptException("could not calculate size of signed message");

        QByteArray result;
        result.resize(outSize);
        if (!CryptSignMessage(&cmsParams, true, 1, inData, inSize, (BYTE *) result.data(), &outSize))
            throw WinCryptException("could not sign message");
        result.resize(outSize);

        cleanup();
        return result;

    } catch (const Exception &) {
        cleanup();
        throw;
    }
}

bool SignaturePrivate::verify(const QByteArray &signaturePkcs7, const QList<QByteArray> &chainOfTrust) throw(Exception)
{
    PCCERT_CONTEXT signerCert = nullptr;
    HCERTSTORE msgCertStore = nullptr;
    HCERTSTORE rootCertStore = nullptr;
    HCERTCHAINENGINE certChainEngine = nullptr;
    PCCERT_CHAIN_CONTEXT chainContext = nullptr;

    auto cleanup = [=]() {
        if (chainContext)
            CertFreeCertificateChain(chainContext);
        if (certChainEngine)
            CertFreeCertificateChainEngine(certChainEngine);
        if (rootCertStore)
            CertCloseStore(rootCertStore, CERT_CLOSE_STORE_FORCE_FLAG);
        if (msgCertStore)
            CertCloseStore(msgCertStore, CERT_CLOSE_STORE_FORCE_FLAG);
        if (signerCert)
            CertFreeCertificateContext(signerCert);
    };

    try {
        CRYPT_VERIFY_MESSAGE_PARA cmsParams;
        memset(&cmsParams, 0, sizeof(cmsParams));
        cmsParams.cbSize = sizeof(cmsParams);
        cmsParams.dwMsgAndCertEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
        cmsParams.hCryptProv = 0;
        cmsParams.pfnGetSignerCertificate = nullptr;
        cmsParams.pvGetArg = nullptr;
        const BYTE *inData[] = { (const BYTE *) hash.constData() };
        DWORD inSize[] = { (DWORD) hash.size() };
        if (!CryptVerifyDetachedMessageSignature(&cmsParams, 0,
                                                 (const BYTE *) signaturePkcs7.constData(),
                                                 signaturePkcs7.size(),
                                                 1, inData, inSize, &signerCert)) {
            throw WinCryptException("Could not read PKCS#7 data");
        }
        if (!signerCert)
            throw WinCryptException("Could not verify signature: no signer certificate");


        msgCertStore = CryptGetMessageCertificates(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, 0,
                                                   (const BYTE *) signaturePkcs7.constData(),
                                                   signaturePkcs7.size());

        if (!msgCertStore)
            throw WinCryptException("Could not retrieve certificates from signature");

        rootCertStore = CertOpenStore(CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                      0, 0, nullptr);
        if (!rootCertStore)
            throw WinCryptException("Could not create temporary root certificate store");

        foreach (const QByteArray &trustedCert, chainOfTrust) {
            // convert from PEM to DER
            DWORD derSize = 0;
            if (!CryptStringToBinaryA(trustedCert.constData(), trustedCert.size(), CRYPT_STRING_BASE64HEADER,
                                      nullptr, &derSize, nullptr, nullptr)) {
                throw WinCryptException("Could not load a certificate from the chain of trust (PEM to DER size calculation failed)");
            }
            QByteArray derBuffer;
            derBuffer.resize(derSize);
            if (!CryptStringToBinaryA(trustedCert.constData(), trustedCert.size(), CRYPT_STRING_BASE64HEADER,
                                      (BYTE *) derBuffer.data(), &derSize, nullptr, nullptr)) {
                throw WinCryptException("Could not load a certificate from the chain of trust (PEM to DER conversion failed)");
            }
            derBuffer.resize(derSize);

            if (!CertAddEncodedCertificateToStore(rootCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                  (const BYTE *) derBuffer.constData(), derBuffer.size(),
                                                  CERT_STORE_ADD_ALWAYS, nullptr)) {
                throw WinCryptException("Could not add a certificate from the chain of trust to the certificate store");
            }
        }

        CERT_CHAIN_ENGINE_CONFIG chainConfig;
        memset(&chainConfig, 0, sizeof(chainConfig));
        chainConfig.cbSize = sizeof(chainConfig);
        chainConfig.hExclusiveRoot = rootCertStore;
        if (!CertCreateCertificateChainEngine(&chainConfig, &certChainEngine))
            throw WinCryptException("Could not create certificate chain");
        CERT_CHAIN_PARA chainParams;
        memset(&chainParams, 0, sizeof(chainParams));
        chainParams.cbSize = sizeof(chainParams);
        if (!CertGetCertificateChain(certChainEngine, signerCert, nullptr, msgCertStore,
                                     &chainParams, 0, nullptr, &chainContext)) {
            throw WinCryptException("Could not verify certificate chain");
        }

        if (chainContext->TrustStatus.dwErrorStatus != CERT_TRUST_NO_ERROR)
            throw WinCryptException("Failed to verify signature");

        cleanup();
        return true;

    } catch (const Exception &) {
        cleanup();
        throw;
    }
}
