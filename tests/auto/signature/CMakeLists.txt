
qt_internal_add_test(tst_signature
    SOURCES
        ../error-checking.h
        tst_signature.cpp
    DEFINES
        AM_TESTSOURCE_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
)

qt_internal_add_resource(tst_signature "signature_testdata"
    PREFIX
        "/"
    FILES
        "signature-openssl.p7"
        "signature-securityframework.p7"
        "signature-wincrypt.p7"
        "signing-no-key.p12"
        "signing.p12"
        "verifying.crt"
)

qt_internal_extend_target(tst_signature CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)

add_custom_target("Other-Files-Signature-Test" SOURCES
    create-test-data.sh
)
