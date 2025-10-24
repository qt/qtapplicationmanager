
qt_internal_add_test(tst_packageextractor
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_packageextractor.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManPackagePrivate
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
)
