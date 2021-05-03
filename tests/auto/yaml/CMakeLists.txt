
qt_internal_add_test(tst_yaml
    SOURCES
        ../error-checking.h
        tst_yaml.cpp
    DEFINES
        AM_TESTDATA_DIR=\\\"${CMAKE_CURRENT_SOURCE_DIR}/../../data/\\\"
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
)

# Resources:
set(qmake_immediate_resource_files
    "data/cache1.yaml"
    "data/cache2.yaml"
    "data/test.yaml"
)

qt_internal_add_resource(tst_yaml "qmake_immediate"
    PREFIX
        "/"
    FILES
        ${qmake_immediate_resource_files}
)


## Scopes:
#####################################################################

qt_internal_extend_target(tst_yaml CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
