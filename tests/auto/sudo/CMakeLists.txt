
qt_internal_add_test(tst_sudo
    SOURCES
        ../error-checking.h
        tst_sudo.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
)

qt_internal_extend_target(tst_sudo CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
