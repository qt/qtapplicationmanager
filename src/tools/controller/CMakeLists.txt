
qt_get_tool_target_name(target_name appman-controller)
qt_internal_add_tool(${target_name}
    USER_FACING
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
    SOURCES
        controller.cpp
        interrupthandler.cpp interrupthandler.h
    LIBRARIES
        Qt::DBus
        Qt::Network
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

if (TARGET ${target_name})
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
endif()
