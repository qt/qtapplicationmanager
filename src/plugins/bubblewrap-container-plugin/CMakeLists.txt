
qt_internal_add_plugin(BubblewrapContainerPlugin
    PLUGIN_TYPE appman_container
    DEFAULT_IF FALSE
    EXCEPTIONS
    SOURCES
        bubblewrapcontainer.cpp bubblewrapcontainer.h
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::DBus
        Qt::AppManPluginInterfacesPrivate
)
