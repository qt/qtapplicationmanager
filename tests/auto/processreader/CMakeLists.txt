
qt_internal_add_test(tst_processreader
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_processreader.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::CorePrivate
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
        Qt::AppManSharedPrivate
        Qt::AppManSystemUIPrivate
)
