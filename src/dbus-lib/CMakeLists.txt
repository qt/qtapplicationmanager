
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManDBusPrivate
    CONFIG_MODULE_NAME appman_dbus
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        dbuscontextadaptor.cpp dbuscontextadaptor.h
        dbusdaemon.cpp dbusdaemon.h
        dbuspolicy.cpp dbuspolicy.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::DBus
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
