
qt_internal_add_test(tst_applicationinfo
    SOURCES
        ../error-checking.h
        tst_applicationinfo.cpp
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManIntentServerPrivate
)

qt_internal_add_resource(tst_applicationinfo "yaml_testdata"
    PREFIX
        "/"
    FILES
        "data/legacy/info.yaml"
        "data/minimal/info.yaml"
        "data/inherit/info.yaml"
        "data/full/info.yaml"
)

qt_internal_extend_target(tst_applicationinfo CONDITION TARGET Qt::DBus
    LIBRARIES
        Qt::DBus
)
