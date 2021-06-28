
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
    LIBRARIES
        Qt::AppManCommonPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
)

qt_internal_extend_target(AppManMonitorPrivate CONDITION LINUX
    SOURCES
        sysfsreader.cpp sysfsreader.h
)
