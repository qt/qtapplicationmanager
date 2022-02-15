/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include "exception.h"
#include "cryptography.h"
#include "signature_p.h"

#include <QtCore/private/qcore_mac_p.h>

#include <Security/SecImportExport.h>
#include <Security/SecPolicy.h>
#include <Security/CMSDecoder.h>
#include <Security/CMSEncoder.h>
#ifdef verify
#  undef verify
#endif

QT_BEGIN_NAMESPACE_AM

class SecurityException : public Exception
{
public:
    SecurityException(OSStatus err, const char *errorString)
        : Exception(Error::Cryptography)
    {
        m_errorString = Cryptography::errorString(err, errorString);
    }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

QByteArray SignaturePrivate::create(const QByteArray &signingCertificatePkcs12,
                                    const QByteArray &signingCertificatePassword) Q_DECL_NOEXCEPT_EXPR(false)
{
    QCFType<SecKeychainRef> localKeyChain;

    auto cleanup = [&localKeyChain]() {
        SecKeychainDelete(localKeyChain);
    };

    try {
        OSStatus err;

        QCFString importPassword = QString::fromUtf8(signingCertificatePassword);
        QByteArray keyChainPassword = Cryptography::generateRandomBytes(16);

        // tempnam() is the best thing we can use here, since we cannot supply a file handle
        if ((err = SecKeychainCreate(tempnam(0, 0), 16, keyChainPassword, false, nullptr, &localKeyChain)))
            throw SecurityException(err, "could not create local key-chain");

        const void *optionKeys[] = { kSecImportExportPassphrase, kSecImportExportKeychain };
        const void *optionValues[] = { importPassword, localKeyChain };

        QCFType<CFDictionaryRef> options = CFDictionaryCreate(0, optionKeys, optionValues, 2, 0, 0);

        QCFType<CFArrayRef> items = CFArrayCreate(0, 0, 0, 0);
        QCFType<CFDataRef> pkcs12Data = signingCertificatePkcs12.toCFData();

        if ((err = SecPKCS12Import(pkcs12Data, options, &items)))
            throw SecurityException(err, "Could not read or not parse PKCS#12 data");

        if (CFArrayGetCount(items) < 1)
            throw SecurityException(err, "Could not find a certificate with a private key within the PKCS#12 data");

        CFDictionaryRef item0 = (CFDictionaryRef) CFArrayGetValueAtIndex(items, 0);
        SecIdentityRef signer = (SecIdentityRef) CFDictionaryGetValue(item0, kSecImportItemIdentity);
        CFArrayRef caCerts = (CFArrayRef) CFDictionaryGetValue(item0, kSecImportItemCertChain);

        QCFType<CMSEncoderRef> encoder;
        if ((err = CMSEncoderCreate(&encoder)))
            throw SecurityException(err, "Failed to create a PKCS#7 encoder");

        Q_ASSERT(encoder);

        if ((err = CMSEncoderSetHasDetachedContent(encoder, true)))
            throw SecurityException(err, "Could not switch PKCS#7 encoder to detached-content mode");

        if ((err = CMSEncoderAddSigners(encoder, signer)))
            throw SecurityException(err, "Cannot add signing certificate to PKCS#7 signature");

        if ((err = CMSEncoderAddSupportingCerts(encoder, caCerts)))
            throw SecurityException(err, "Cannot add CA certificates to PKCS#7 signature");

        if ((err = CMSEncoderUpdateContent(encoder, hash.constData(), hash.size())))
            throw SecurityException(err, "Cannot add hash value to PKCS#7 signature");

        QCFType<CFDataRef> pkcs7Der;
        if ((err = CMSEncoderCopyEncodedContent(encoder, &pkcs7Der)))
            throw SecurityException(err, "Failed to create PKCS#7 signature");

        cleanup();
        return QByteArray::fromCFData(pkcs7Der);

    } catch (const Exception &) {
        cleanup();
        throw;
    }
}

bool SignaturePrivate::verify(const QByteArray &signaturePkcs7,
                              const QList<QByteArray> &chainOfTrust) Q_DECL_NOEXCEPT_EXPR(false)
{
    OSStatus err;

    QCFType<CMSDecoderRef> decoder;
    if ((err = CMSDecoderCreate(&decoder)))
        throw SecurityException(err, "Count not create a PKCS#7 decoder");

    Q_ASSERT(decoder);

    if ((err = CMSDecoderUpdateMessage(decoder, signaturePkcs7.constData(), signaturePkcs7.size())))
        throw SecurityException(err, "Could not read PKCS#7 data");

    if ((err = CMSDecoderFinalizeMessage(decoder)))
        throw SecurityException(err, "Could not decode PKCS#7 signature");

    QCFType<CFDataRef> hashContent = hash.toCFData();
    if ((err = CMSDecoderSetDetachedContent(decoder, hashContent)))
        throw SecurityException(err, "Could not set PKCS#7 signature detached content");

    QCFType<CFMutableArrayRef> caCerts = CFArrayCreateMutable(nullptr, 0, nullptr);
    for (const QByteArray &trustedCert : chainOfTrust) {
        QCFType<CFArrayRef> certs;
        SecExternalFormat itemFormat = kSecFormatUnknown; // X509Cert;
        SecExternalItemType itemType = kSecItemTypeUnknown; //Certificate;
        if ((err = SecItemImport(trustedCert.toCFData(), nullptr, &itemFormat, &itemType, 0, nullptr, nullptr, &certs)))
            throw SecurityException(err, "Could not load a certificate from the chain of trust");

        for (int i = 0 ; i < CFArrayGetCount(certs); ++i) {
            if (CFGetTypeID(CFArrayGetValueAtIndex(certs, i)) != SecCertificateGetTypeID())
                continue;
            CFArrayAppendValue(caCerts, CFRetain(CFArrayGetValueAtIndex(certs, i)));
        }
    }

    QCFType<CFArrayRef> msgCerts;
    if ((err = CMSDecoderCopyAllCerts(decoder, &msgCerts)))
        throw SecurityException(err, "Could not retrieve certificates from message");

    QCFType<SecPolicyRef> secPolicy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, nullptr);
    CMSSignerStatus signerStatusOut = kCMSSignerUnsigned;
    QCFType<SecTrustRef> trustRef;

    if ((err = CMSDecoderCopySignerStatus(decoder, 0, secPolicy, false, &signerStatusOut, &trustRef, nullptr)))
        throw SecurityException(err, "Failed to verify signature");

    if (signerStatusOut != kCMSSignerValid)
        throw SecurityException(err, "No valid signer certificate found");

    if ((err = SecTrustSetAnchorCertificates(trustRef, caCerts)))
        throw SecurityException(err, "Could not set custom trust anchor");

    SecTrustResultType trustResult;
    if ((err = SecTrustEvaluate(trustRef, &trustResult)))
        throw SecurityException(err, "Could not evaluate chain of trust");

    if (trustResult != kSecTrustResultUnspecified)
        throw SecurityException(err, "Failed to verify signature (no chain of trust)");

    return true;
}

#pragma clang diagnostic pop

QT_END_NAMESPACE_AM
