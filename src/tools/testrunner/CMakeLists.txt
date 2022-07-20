
qt_get_tool_target_name(target_name appman-qmltestrunner)
qt_internal_add_tool(${target_name}
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
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

if (TARGET ${target_name})
    include(QtAppManBuildConfig)

    qtam_internal_add_build_config(${target_name})
endif()
