
qt_internal_add_test(tst_signature
    SOURCES
        ../error-checking.h
        tst_signature.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
)

# Resources:
set(tst_signature_resource_files
    "signature-openssl.p7"
    "signature-securityframework.p7"
    "signature-wincrypt.p7"
    "signing-no-key.p12"
    "signing.p12"
    "verifying.crt"
)

qt_internal_add_resource(tst_signature "tst_signature"
    PREFIX
        "/"
    FILES
        ${tst_signature_resource_files}
)


#### Keys ignored in scope 1:.:.:signature.pro:<TRUE>:
# OTHER_FILES = "create-test-data.sh"

## Scopes:
#####################################################################

qt_internal_extend_target(tst_signature CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
