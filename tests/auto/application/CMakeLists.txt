
qt_internal_add_test(tst_application
    SOURCES
        ../error-checking.h
        tst_application.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_BINARY_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
)

# Resources:
set(tst_application_resource_files
    "info.yaml"
)

qt_internal_add_resource(tst_application "tst_application"
    PREFIX
        "/"
    FILES
        ${tst_application_resource_files}
)

qt_internal_extend_target(tst_application CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
