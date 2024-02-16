
qt_internal_add_app(appman-qmltestrunner
    EXCEPTIONS
    SOURCES
        ../appman/appman.cpp
        amtest.cpp amtest.h
        testrunner.cpp testrunner.h
    DEFINES
        AM_TESTRUNNER
        AM_COMPILING_APPMAN
    LIBRARIES
        Qt::QuickTest
        Qt::QuickTestPrivate
        Qt::TestPrivate
        Qt::AppManMainPrivate
)

set_target_properties(appman-qmltestrunner PROPERTIES WIN32_EXECUTABLE FALSE)

include(QtAppManBuildConfig)

qtam_internal_add_build_config(appman-qmltestrunner)
