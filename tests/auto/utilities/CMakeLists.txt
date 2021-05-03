
qt_internal_add_test(tst_utilities
    SOURCES
        ../error-checking.h
        tst_utilities.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
)

## Scopes:
#####################################################################

qt_internal_extend_target(tst_utilities CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
