
qt_internal_add_test(tst_debugwrapper
    SOURCES
        ../error-checking.h
        tst_debugwrapper.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManManagerPrivate
)

## Scopes:
#####################################################################

qt_internal_extend_target(tst_debugwrapper CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
