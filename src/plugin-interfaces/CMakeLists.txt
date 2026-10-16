
qt_internal_add_module(AppManPluginInterfacesPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    PLUGIN_TYPES
        appman_container
        appman_startup
    SOURCES
        containerinterface.cpp containerinterface.h
        startupinterface.cpp startupinterface.h
    PUBLIC_LIBRARIES
        Qt::Core
)

# keep the pre-6.11 qmake module id (Squish for example relies on that)
set_target_properties(AppManPluginInterfacesPrivate PROPERTIES
    _qt_config_module_name appman_plugininterfaces_private)
