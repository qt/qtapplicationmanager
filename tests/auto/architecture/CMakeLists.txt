
qt_internal_add_test(tst_architecture
    SOURCES
        ../error-checking.h
        tst_architecture.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::AppManCommonPrivate
)
