
qt_internal_add_test(tst_packager-tool
    SOURCES
        ../../../src/tools/packager/packagingjob.cpp
        ../error-checking.h
        tst_packager-tool.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    INCLUDE_DIRECTORIES
        ../../../src/tools/packager
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
        Qt::AppManManagerPrivate
        Qt::AppManPackagePrivate
)

qt_internal_extend_target(tst_packager-tool CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
