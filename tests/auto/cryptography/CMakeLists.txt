
qt_internal_add_test(tst_cryptography
    SOURCES
        ../error-checking.h
        tst_cryptography.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
)

qt_internal_extend_target(tst_cryptography CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
