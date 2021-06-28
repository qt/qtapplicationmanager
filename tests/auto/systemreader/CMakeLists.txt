
qt_internal_add_test(tst_systemreader
    SOURCES
        ../error-checking.h
        tst_systemreader.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/\\\"
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

qt_internal_extend_target(tst_systemreader CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
