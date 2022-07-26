
qt_internal_add_test(tst_runtime
    SOURCES
        ../error-checking.h
        tst_runtime.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    LIBRARIES
        Qt::Network
        Qt::Qml
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
)

qt_internal_extend_target(tst_runtime CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
