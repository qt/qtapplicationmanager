
qt_internal_add_test(tst_main
    SOURCES
        ../error-checking.h
        tst_main.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManMainPrivate
        Qt::AppManManagerPrivate
)

# Resources:
set(main_resource_files
    "dummy.qml"
)

qt_internal_add_resource(tst_main "main"
    PREFIX
        "/foo"
    FILES
        ${main_resource_files}
)

qt_internal_extend_target(tst_main CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
