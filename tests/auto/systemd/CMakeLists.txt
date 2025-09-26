
qt_internal_add_test(tst_systemd
    SOURCES
        ../error-checking.h
        tst_systemd.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
)
