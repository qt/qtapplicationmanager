
qt_internal_add_test(tst_packageextractor
    SOURCES
        ../error-checking.h
        tst_packageextractor.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
)

qt_internal_extend_target(tst_packageextractor CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
