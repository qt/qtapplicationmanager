
qt_internal_add_test(tst_signature
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_signature.cpp
    DEFINES
        AM_TESTSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
    BUILTIN_TESTDATA
    TESTDATA
        signature-openssl.p7
        signature-securityframework.p7
        signature-wincrypt.p7
        signature-legacy-sha1.p7
        signing-no-key.p12
        signing.p12
        verifying.crt
        root-ca.info
        dev-ca.info
        dev.info
)

add_custom_target("Other-Files-Signature-Test" SOURCES
    create-test-data.sh
)
