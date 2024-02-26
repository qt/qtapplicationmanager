
qt_internal_add_test(tst_packagemanager
    SOURCES
        ../error-checking.h
        tst_packagemanager.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManPackagePrivate
)

qt_internal_extend_target(tst_packagemanager CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
