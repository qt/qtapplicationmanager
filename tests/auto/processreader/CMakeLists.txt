
qt_internal_add_test(tst_processreader
    SOURCES
        ../error-checking.h
        tst_processreader.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManWindowPrivate
)

## Scopes:
#####################################################################

qt_internal_extend_target(tst_processreader CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
