# Generated from controller.pro.

#####################################################################
## appman-controller Tool:
#####################################################################

qt_get_tool_target_name(target_name appman-controller)
qt_internal_add_tool(${target_name}
    USER_FACING
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
    SOURCES
        controller.cpp
        interrupthandler.cpp interrupthandler.h
    PUBLIC_LIBRARIES
        Qt::DBus
        Qt::Network
        Qt::AppManCommonPrivate
)
qt_internal_extend_target(${target_name}
    DBUS_INTERFACE_SOURCES
        ../../dbus-lib/io.qt.packagemanager.xml
)
qt_internal_extend_target(${target_name}
    DBUS_INTERFACE_SOURCES
        ../../dbus-lib/io.qt.applicationmanager.xml
    DBUS_INTERFACE_FLAGS
        -i dbus-utilities.h
)
