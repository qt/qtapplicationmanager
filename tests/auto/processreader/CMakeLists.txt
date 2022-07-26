
qt_internal_add_test(tst_processreader
    SOURCES
        ../error-checking.h
        tst_processreader.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManWindowPrivate
)

qt_internal_extend_target(tst_processreader CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
