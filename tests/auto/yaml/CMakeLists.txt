
qt_internal_add_test(tst_yaml
    SOURCES
        ../error-checking.h
        tst_yaml.cpp
    DEFINES
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
)

qt_internal_add_resource(tst_yaml "yaml_testdata"
    PREFIX
        "/"
    FILES
        "data/cache1.yaml"
        "data/cache2.yaml"
        "data/test.yaml"
)

qt_internal_extend_target(tst_yaml CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
