
qt_internal_add_test(tst_package-server-tool
    SOURCES
        ../error-checking.h
        tst_package-server-tool.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
)
