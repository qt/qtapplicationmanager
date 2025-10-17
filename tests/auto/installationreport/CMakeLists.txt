
qt_internal_add_test(tst_installationreport
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_installationreport.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
)
