
qt_internal_add_test(tst_packagecreator
    SOURCES
        ../error-checking.h
        tst_packagecreator.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
)

qt_internal_extend_target(tst_packagecreator CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
