
qt_internal_add_app(appman-controller
    EXCEPTIONS
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

qt_internal_extend_target(appman-controller
    DBUS_INTERFACE_SOURCES
        ${CMAKE_SOURCE_DIR}/dbus-interfaces/io.qt.packagemanager.xml
)
qt_internal_extend_target(appman-controller
    DBUS_INTERFACE_SOURCES
        ${CMAKE_SOURCE_DIR}/dbus-interfaces/io.qt.applicationmanager.xml
    DBUS_INTERFACE_FLAGS
        -i dbus-utilities.h
)
