
qt_internal_add_module(AppManPluginInterfacesPrivate
    EXCEPTIONS
    PLUGIN_TYPES
        appman_container
        appman_startup
    SOURCES
        containerinterface.cpp containerinterface.h
        startupinterface.cpp startupinterface.h
    PUBLIC_LIBRARIES
        Qt::Core
)
