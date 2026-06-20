
qt_internal_add_test(tst_packager-tool
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        ../toolrunner.h
        tst_packager-tool.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
        Qt::AppManSystemUIPrivate
)
