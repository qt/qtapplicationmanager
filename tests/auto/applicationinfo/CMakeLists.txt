
qt_internal_add_test(tst_applicationinfo
    SOURCES
        ../error-checking.h
        tst_applicationinfo.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
)

## Scopes:
#####################################################################

qt_internal_extend_target(tst_applicationinfo CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
