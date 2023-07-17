
qt_internal_add_test(tst_controller-tool
    SOURCES
        ../error-checking.h
        tst_controller-tool.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::DBus
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManMainPrivate
        Qt::AppManManagerPrivate
)
