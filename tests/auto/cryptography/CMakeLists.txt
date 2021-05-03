
qt_internal_add_test(tst_cryptography
    SOURCES
        ../error-checking.h
        tst_cryptography.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
)

## Scopes:
#####################################################################

qt_internal_extend_target(tst_cryptography CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
