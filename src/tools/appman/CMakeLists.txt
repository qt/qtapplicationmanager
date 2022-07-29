
qt_internal_add_app(appman
    EXCEPTIONS
    SOURCES
        appman.cpp
    LIBRARIES
        Qt::AppManMainPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

include(QtAppManBuildConfig)

qtam_internal_add_build_config(appman)
