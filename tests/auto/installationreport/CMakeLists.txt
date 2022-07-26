
qt_internal_add_test(tst_installationreport
    SOURCES
        ../error-checking.h
        tst_installationreport.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
)

qt_internal_extend_target(tst_installationreport CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
