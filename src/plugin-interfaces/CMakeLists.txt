
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManPluginInterfacesPrivate
    CONFIG_MODULE_NAME appman_plugininterfaces
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        containerinterface.cpp containerinterface.h
        startupinterface.cpp startupinterface.h
    PUBLIC_LIBRARIES
        Qt::Core
)
