// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#include <QtEndian>

#include "exception.h"
#include "signature_p.h"

#include <windows.h>
#include <wincrypt.h>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

class WinCryptException : public Exception  // clazy:exclude=copyable-polymorphic
{
public:
    WinCryptException(const char *errorString)
        : Exception(Error::Cryptography, errorString)
    {
        if (auto err = ::GetLastError()) {
            LPWSTR msg = nullptr;
            ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, (LPWSTR) &msg, 0, nullptr);
            // remove potential \r\n at the end
            const QString msgStr = QString::fromWCharArray(msg).trimmed();
            ::HeapFree(::GetProcessHeap(), 0, msg);

            if (!msgStr.isEmpty())
                m_errorString += u": " + msgStr;
            else
                m_errorString += u": error #" + QString::number(int(err));
        }
    }
};

static QString certTrustStatusToString(const CERT_TRUST_STATUS &cts)
{
    switch (cts.dwErrorStatus) {
    case CERT_TRUST_NO_ERROR:
        return { };
    case CERT_TRUST_IS_NOT_TIME_VALID:
        return u"expired"_s;
    case CERT_TRUST_IS_REVOKED:
        return u"revoked"_s;
    default:
        return u"error: 0x%1, info: 0x%2"_s
            .arg(cts.dwErrorStatus, 8, 16, QChar(u'0'))
            .arg(cts.dwInfoStatus, 8, 16, QChar(u'0'));
    }
}

static QByteArrayList importPEMasDER(const QByteArray &pem)
{
    // Convert from PEM to DER. PEM can contain multiple items, but Windows can only
    // import the first one it sees. The only way around is to split the PEM ourselves

    QByteArrayList result;
    static const QByteArray beginMarker = "-----BEGIN "_ba;
    static const QByteArray endMarker   = "-----END "_ba;
    static const QByteArray finalMarker = "-----"_ba;

    qsizetype pos = 0;
    while (true) {
        auto beginPos = pem.indexOf(beginMarker, pos);
        if (beginPos < 0)
            break;
        auto endPos = pem.indexOf(endMarker, beginPos);
        if (endPos < 0)
            break;
        auto finalPos = pem.indexOf(finalMarker, endPos + endMarker.size());
        if (finalPos < 0)
            break;
        finalPos += finalMarker.size();
        auto pemItem = QByteArrayView(pem).slice(beginPos, finalPos - beginPos);
        pos = finalPos;

        DWORD derSize = 0;
        if (!::CryptStringToBinaryA(pemItem.constData(), pemItem.size(), CRYPT_STRING_BASE64HEADER,
                                    nullptr, &derSize, nullptr, nullptr)) {
            throw WinCryptException("PEM to DER size calculation failed");
        }
        QByteArray derBuffer;
        derBuffer.resize(derSize);
        if (!::CryptStringToBinaryA(pemItem.constData(), pemItem.size(), CRYPT_STRING_BASE64HEADER,
                                    (BYTE *) derBuffer.data(), &derSize, nullptr, nullptr)) {
            throw WinCryptException("PEM to DER conversion failed");
        }
        derBuffer.resize(derSize);
        result << derBuffer;
    }
    if (result.isEmpty())
        throw Exception("not a PEM file");
    return result;
}

// copied from qfilesystemengine_win.cpp:
static inline QDateTime fileTimeToQDateTime(const FILETIME *time)
{
    if (time->dwHighDateTime == 0 && time->dwLowDateTime == 0)
        return QDateTime();

    SYSTEMTIME sTime;
    FileTimeToSystemTime(time, &sTime);
    return QDateTime(QDate(sTime.wYear, sTime.wMonth, sTime.wDay),
                     QTime(sTime.wHour, sTime.wMinute, sTime.wSecond, sTime.wMilliseconds),
                     QTimeZone::UTC);
}

class CertificateParser
{
public:
    static Certificate parseCertContext(PCCERT_CONTEXT cert);
};

Certificate CertificateParser::parseCertContext(PCCERT_CONTEXT cert)
{
    if (!cert)
        return { };

    auto reverseBitsInByte = [](BYTE &b) {
        // https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64Bits
        b = ((b * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
    };

    auto integerBlobToHexString = [](DWORD size, const BYTE *data) {
        QByteArray s;
        s.resize(size);
        for (DWORD i = 0; i < size; ++i)
            s[i] = reinterpret_cast<const char *>(data)[size - i - 1];
        return QString::fromLatin1(s.toHex());
    };

    Certificate info;
    info.m_serialNumber = integerBlobToHexString(cert->pCertInfo->SerialNumber.cbData,
                                                 cert->pCertInfo->SerialNumber.pbData);
    info.m_validityNotAfter = fileTimeToQDateTime(&cert->pCertInfo->NotAfter);
    info.m_validityNotBefore = fileTimeToQDateTime(&cert->pCertInfo->NotBefore);

    BYTE keyUsage[2];
    if (::CertGetIntendedKeyUsage(X509_ASN_ENCODING, cert->pCertInfo, keyUsage, sizeof(keyUsage))) {
        // This is the raw ASN.1 bit-string, but it has the bit order backwards
        reverseBitsInByte(keyUsage[0]);
        reverseBitsInByte(keyUsage[1]);
        info.m_keyUsages = Certificate::KeyUsages::fromInt(qFromLittleEndian<quint16>(keyUsage));
    }

    DWORD sha256Len = 256 / 8;
    QByteArray shaBuffer(sha256Len, '\0');
    if (!::CertGetCertificateContextProperty(cert, CERT_SHA256_HASH_PROP_ID, shaBuffer.data(), &sha256Len)
            || (sha256Len != shaBuffer.size())) {
        throw WinCryptException("could not calculate SHA-256 fingerprint");
    }
    info.m_fingerprint = shaBuffer;

    QStringList subjectAlternativeNames;
    if (PCERT_EXTENSION sanExt = ::CertFindExtension(szOID_SUBJECT_ALT_NAME2,
                                                     cert->pCertInfo->cExtension,
                                                     cert->pCertInfo->rgExtension)) {
        CERT_ALT_NAME_INFO *sanInfo = nullptr;
        DWORD sanInfoSize = 0;
        if (::CryptDecodeObjectEx(X509_ASN_ENCODING, szOID_SUBJECT_ALT_NAME2,
                                  sanExt->Value.pbData, sanExt->Value.cbData,
                                  CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG, nullptr,
                                  &sanInfo, &sanInfoSize)) {
            for (DWORD i = 0; i < sanInfo->cAltEntry; ++i) {
                if (sanInfo->rgAltEntry[i].dwAltNameChoice == CERT_ALT_NAME_URL)
                    subjectAlternativeNames << QString::fromWCharArray(sanInfo->rgAltEntry[i].pwszDNSName);
            }
            ::LocalFree(sanInfo);
        }
    }
    info.m_subjectAlternativeNames = subjectAlternativeNames;

    QVariantMap subject;
    CERT_NAME_INFO *nameInfo = nullptr;
    DWORD nameInfoSize = 0;
    if (::CryptDecodeObjectEx(X509_ASN_ENCODING, X509_NAME,
                              cert->pCertInfo->Subject.pbData, cert->pCertInfo->Subject.cbData,
                              CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG, nullptr,
                              &nameInfo, &nameInfoSize)) {
        for (DWORD i = 0; i < nameInfo->cRDN; ++i) {
            for (DWORD j = 0; j < nameInfo->rgRDN[i].cRDNAttr; ++j) {
                auto &a = nameInfo->rgRDN[i].rgRDNAttr[j];
                DWORD bufferSize = ::CertRDNValueToStrW(a.dwValueType, &a.Value, nullptr, 0);
                if (bufferSize <= 0)
                    throw Exception("Error parsing certificate distinguished name");
                auto buffer = std::make_unique<WCHAR[]>(bufferSize);

                auto nameSize = ::CertRDNValueToStrW(a.dwValueType, &a.Value, buffer.get(), bufferSize);
                if ((nameSize <= 0) || (nameSize > bufferSize))
                    throw Exception("Error retrieving certificate distinguished name");

                const QString oid = QString::fromLatin1(a.pszObjId);
                const QString value = QString::fromWCharArray(buffer.get(), nameSize - 1);

                SignaturePrivate::setDNByOid(subject, oid, value);
            }
        }
        ::LocalFree(nameInfo);
    }
    info.m_subject = subject;
    return info;
}

QByteArray SignaturePrivate::create(const QByteArray &signingCertificatePkcs12,
                                    const QByteArray &signingCertificatePassword,
                                    const std::function<void(const Certificate &)> &checkCertificate)
{
    HCERTSTORE certStore = nullptr;
    QVector<PCCERT_CONTEXT> signCerts;
    QVector<PCCERT_CONTEXT> allCerts;

    auto cleanup = qScopeGuard([&] {
        for (auto cert : std::as_const(allCerts))
            ::CertFreeCertificateContext(cert);
        if (certStore)
            ::CertCloseStore(certStore, CERT_CLOSE_STORE_FORCE_FLAG);
    });

    // Although WinCrypt could, the macOS Security Framework cannot process empty detached data
    if (hash.isEmpty())
        throw Exception("cannot sign an empty hash value");

    ::CRYPT_DATA_BLOB pkcs12Blob;
    pkcs12Blob.cbData = signingCertificatePkcs12.size();
    pkcs12Blob.pbData = (BYTE *) signingCertificatePkcs12.constData();

    QString password = QString::fromUtf8(signingCertificatePassword);

    certStore = ::PFXImportCertStore(&pkcs12Blob,
                                     reinterpret_cast<const wchar_t *>(password.utf16()),
                                     PKCS12_NO_PERSIST_KEY | PKCS12_PREFER_CNG_KSP);
    if (!certStore)
        throw WinCryptException("could not read or not parse PKCS#12 certificate");

    PCCERT_CONTEXT cert = nullptr;
    while ((cert = ::CertEnumCertificatesInStore(certStore, cert))) {
        auto certCopy = ::CertDuplicateCertificateContext(cert);
        allCerts << certCopy;

        BYTE keyUsage = 0;
        if (!::CertGetIntendedKeyUsage(X509_ASN_ENCODING, cert->pCertInfo, &keyUsage, sizeof(keyUsage))
            || !(keyUsage & CERT_KEY_CERT_SIGN_KEY_USAGE)) {
            signCerts << certCopy;
        }
    }

    if (signCerts.size() != 1)
        throw Exception("PKCS#12 key did not contain exactly 1 signing certificate");

    if (checkCertificate)
        checkCertificate(CertificateParser::parseCertContext(signCerts.constFirst()));

    ::CRYPT_SIGN_MESSAGE_PARA cmsParams;
    memset(&cmsParams, 0, sizeof(cmsParams));
    cmsParams.cbSize = sizeof(cmsParams);
    cmsParams.dwMsgEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    cmsParams.pSigningCert = signCerts.first();
    cmsParams.HashAlgorithm.pszObjId = (LPSTR) szOID_NIST_sha256;
    cmsParams.cMsgCert = allCerts.size();
    cmsParams.rgpMsgCert = allCerts.data();
    cmsParams.dwFlags = CRYPT_MESSAGE_SILENT_KEYSET_FLAG;
    const BYTE *inData[] = { (const BYTE *) hash.constData() };
    DWORD inSize[] = { (DWORD) hash.size() };
    DWORD outSize = 0;
    if (!::CryptSignMessage(&cmsParams, true, 1, inData, inSize, nullptr, &outSize))
        throw WinCryptException("could not calculate size of signed message");

    QByteArray result;
    result.resize(outSize);
    if (!::CryptSignMessage(&cmsParams, true, 1, inData, inSize, (BYTE *) result.data(), &outSize))
        throw WinCryptException("could not sign message");
    result.resize(outSize);

    return result;
}

Signature::VerificationResult SignaturePrivate::verify(const QByteArray &signaturePkcs7,
                                                       const QByteArrayList &trustedCertsData)
{
    PCCERT_CONTEXT signerCert = nullptr;
    HCERTSTORE msgCertStore = nullptr;
    HCERTSTORE rootCertStore = nullptr;
    HCERTCHAINENGINE certChainEngine = nullptr;
    PCCERT_CHAIN_CONTEXT chainContext = nullptr;

    QHash<PCCERT_CONTEXT, Signature::CertificateRole> certContextToRole;

    auto cleanup = qScopeGuard([&] {
        for (auto cert : certContextToRole.keys())
            ::CertFreeCertificateContext(cert);
        if (chainContext)
            ::CertFreeCertificateChain(chainContext);
        if (certChainEngine)
            ::CertFreeCertificateChainEngine(certChainEngine);
        if (rootCertStore)
            ::CertCloseStore(rootCertStore, CERT_CLOSE_STORE_FORCE_FLAG);
        if (msgCertStore)
            ::CertCloseStore(msgCertStore, CERT_CLOSE_STORE_FORCE_FLAG);
        if (signerCert)
            ::CertFreeCertificateContext(signerCert);
    });

    ::CRYPT_VERIFY_MESSAGE_PARA cmsParams;
    memset(&cmsParams, 0, sizeof(cmsParams));
    cmsParams.cbSize = sizeof(cmsParams);
    cmsParams.dwMsgAndCertEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    cmsParams.hCryptProv = 0;
    cmsParams.pfnGetSignerCertificate = nullptr;
    cmsParams.pvGetArg = nullptr;
    const BYTE *inData[] = { (const BYTE *) hash.constData() };
    DWORD inSize[] = { (DWORD) hash.size() };
    if (!::CryptVerifyDetachedMessageSignature(&cmsParams, 0,
                                               (const BYTE *) signaturePkcs7.constData(),
                                               signaturePkcs7.size(),
                                               1, inData, inSize, &signerCert)) {
        throw WinCryptException("Failed to verify PKCS#7 signature");
    }
    if (!signerCert)
        throw WinCryptException("Failed to verify signature: no signer certificate");

    msgCertStore = ::CryptGetMessageCertificates(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, 0,
                                                 (const BYTE *) signaturePkcs7.constData(),
                                                 signaturePkcs7.size());
    if (!msgCertStore)
        throw WinCryptException("Could not retrieve certificates from signature");

    rootCertStore = ::CertOpenStore(CERT_STORE_PROV_MEMORY, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                    0, 0, nullptr);
    if (!rootCertStore)
        throw WinCryptException("Could not create temporary root certificate store");

    for (const QByteArray &trustedCertData : trustedCertsData) {
        QByteArrayList trustedCertList;
        try {
            trustedCertList = importPEMasDER(trustedCertData);
        } catch (const Exception &e) {
            throw Exception("Could not load a certificate from the chain of trust: %1").arg(e.errorString());
        }
        for (const QByteArray &trustedCert : std::as_const(trustedCertList)) {
            PCCERT_CONTEXT cert = nullptr;
            if (!::CertAddEncodedCertificateToStore(rootCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                    (const BYTE *) trustedCert.constData(), trustedCert.size(),
                                                    CERT_STORE_ADD_ALWAYS, &cert)) {
                throw WinCryptException("Could not add a certificate from the chain of trust to the certificate store");
            }

            if (auto it = requiredRoles.constFind(trustedCertData); it != requiredRoles.constEnd())
                certContextToRole.insert(cert, it.value());
            else
                ::CertFreeCertificateContext(cert);
        }
    }

    bool hasCRLs = false;
    for (const QByteArray &requiredCRLPEM : std::as_const(requiredCRLs)) {
        QByteArrayList requiredCRLList;
        try {
            requiredCRLList = importPEMasDER(requiredCRLPEM);
        } catch (const Exception &e) {
            throw Exception("Could not load a CRL: %1").arg(e.errorString());
        }

        for (const QByteArray &requiredCRL : std::as_const(requiredCRLList)) {
            if (!::CertAddEncodedCRLToStore(rootCertStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                            (const BYTE *) requiredCRL.constData(), requiredCRL.size(),
                                            CERT_STORE_ADD_ALWAYS, nullptr)) {
                throw WinCryptException("Could not add a CRL to the certificate store");
            }
            hasCRLs = true;
        }
    }

    ::CERT_CHAIN_ENGINE_CONFIG chainConfig;
    memset(&chainConfig, 0, sizeof(chainConfig));
    chainConfig.cbSize = sizeof(chainConfig);
    chainConfig.hExclusiveRoot = rootCertStore;
    if (!::CertCreateCertificateChainEngine(&chainConfig, &certChainEngine))
        throw WinCryptException("Could not create certificate chain");
    CERT_CHAIN_PARA chainParams;
    memset(&chainParams, 0, sizeof(chainParams));
    chainParams.cbSize = sizeof(chainParams);
    DWORD verificationFlags = 0;
    if (hasCRLs)
        verificationFlags |= (CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY | CERT_CHAIN_REVOCATION_CHECK_CHAIN);

    if (!::CertGetCertificateChain(certChainEngine, signerCert, nullptr, msgCertStore,
                                   &chainParams, verificationFlags, nullptr,
                                   &chainContext)) {
        throw WinCryptException("Could not verify certificate chain");
    }

    if (chainContext->TrustStatus.dwErrorStatus != CERT_TRUST_NO_ERROR) {
        throw Exception("Failed to verify signature: %1")
            .arg(certTrustStatusToString(chainContext->TrustStatus));
    }

    if ((chainContext->cChain != 1) || (chainContext->rgpChain[0]->cElement < 2))
        throw Exception("Invalid verification chain");
    if (chainContext->rgpChain[0]->rgpElement[0]->pCertContext != signerCert)
        throw Exception("Invalid verification chain: does not contain signer certificate");

    // Parse all certificates in the verification chain
    // The chain contains: [0]=signer, [1..n-1]=intermediates, [n-1]=root
    QList<Certificate> chain;
    DWORD chainContextLength = chainContext->rgpChain[0]->cElement;
    for (DWORD i = 0; i < chainContextLength; ++i) {
        PCCERT_CONTEXT chainCert = chainContext->rgpChain[0]->rgpElement[i]->pCertContext;
        if (!chainCert)
            throw Exception("Invalid certificate at position %1 in the verification chain").arg(i);

        try {
            const auto c = CertificateParser::parseCertContext(chainCert);
            if ((i >= 1) && !requiredRoles.isEmpty()) {
                // Find the matching trusted certificate by comparing certificate contexts
                Signature::CertificateRole role;
                bool found = false;
                for (const auto &[pc, r] : certContextToRole.asKeyValueRange()) {
                    if (::CertCompareCertificate(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                            chainCert->pCertInfo, pc->pCertInfo)) {
                        role = r;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    throw Exception("Certificate at position %1 in the verification chain is not trusted").arg(i);
                verifyCertificateRole(c.subjectAsString(), i, chainContextLength, role);
            }
            chain.append(c);
        } catch (const Exception &e) {
            throw Exception("Could not parse certificate at position %1 in the verification chain: %2")
                .arg(i).arg(e.errorString());
        }
    }

    return { chain };
}

QT_END_NAMESPACE_AM
