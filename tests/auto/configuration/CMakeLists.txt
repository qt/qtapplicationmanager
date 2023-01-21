
qt_internal_add_test(tst_configuration
    SOURCES
        ../error-checking.h
        tst_configuration.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManMainPrivate
)

qt_internal_add_resource(tst_configuration "qmake_immediate"
    PREFIX
        "/"
    FILES
        "data/config1.yaml"
        "data/config2.yaml"
        "data/empty.yaml"
)

qt_internal_extend_target(tst_configuration CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
