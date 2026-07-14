
qt_internal_add_test(tst_yaml
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_yaml.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManSystemUIPrivate
    BUILTIN_TESTDATA
    TESTDATA
        data/test.yaml
        data/tricky.yaml
)
