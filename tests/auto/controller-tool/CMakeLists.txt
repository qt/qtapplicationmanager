
qt_internal_add_test(tst_controller-tool
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_controller-tool.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::DBus
        Qt::Network
        Qt::AppManMainPrivate
    BUILTIN_TESTDATA
    TESTDATA
        BLACKLIST
)
