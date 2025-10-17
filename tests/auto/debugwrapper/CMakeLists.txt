
qt_internal_add_test(tst_debugwrapper
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_debugwrapper.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManManagerPrivate
)
