
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManMonitorPrivate
    CONFIG_MODULE_NAME appman_monitor
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        processreader.cpp processreader.h
        systemreader.cpp systemreader.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_extend_target(AppManMonitorPrivate CONDITION (LINUX OR ANDROID)
    SOURCES
        sysfsreader.cpp sysfsreader.h
)
