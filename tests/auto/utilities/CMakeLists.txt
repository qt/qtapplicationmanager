
qt_internal_add_test(tst_utilities
    SOURCES
        ../error-checking.h
        tst_utilities.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
)

qt_internal_extend_target(tst_utilities CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
