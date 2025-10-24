
qt_internal_add_qml_module(AppManTestQmlPrivate
    URI QtApplicationManager.Test
    VERSION "${PROJECT_VERSION}"
    STATIC
    NO_PLUGIN
    NO_PRIVATE_MODULE
    EXCEPTIONS
    SOURCES
        amtest.cpp amtest.h
    DEPENDENCIES
        QtQuick
    IMPORTS
        QtApplicationManager/auto
        QtQuick
    PUBLIC_LIBRARIES
        Qt::TestPrivate
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_add_app(appman-qmltestrunner
    EXCEPTIONS
    SOURCES
        ../appman/appman.cpp
        testrunner.cpp testrunner.h
    DEFINES
        AM_TESTRUNNER
        AM_COMPILING_APPMAN
    LIBRARIES
        Qt::QuickTest
        Qt::QuickTestPrivate
        Qt::AppManTestQmlPrivate
        Qt::AppManSystemUIPrivate
)

set_target_properties(appman-qmltestrunner PROPERTIES WIN32_EXECUTABLE FALSE)

include(QtAppManBuildConfig)

qtam_internal_add_build_config(appman-qmltestrunner)
