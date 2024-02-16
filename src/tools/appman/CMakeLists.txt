
qt_internal_add_app(appman
    EXCEPTIONS
    SOURCES
        appman.cpp
    LIBRARIES
        Qt::AppManMainPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

set_target_properties(appman PROPERTIES WIN32_EXECUTABLE FALSE)

include(QtAppManBuildConfig)

qtam_internal_add_build_config(appman)
