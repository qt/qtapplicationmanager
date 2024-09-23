
qt_internal_add_module(AppManApplicationMainPrivate
    CONFIG_MODULE_NAME appman_application_main
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        applicationmain.cpp applicationmain.h
        dbusapplicationinterfaceimpl.cpp dbusapplicationinterfaceimpl.h
        dbusnotificationimpl.cpp dbusnotificationimpl.h
        intentclientdbusimplementation.cpp intentclientdbusimplementation.h
        waylandapplicationmanagerwindowimpl.cpp waylandapplicationmanagerwindowimpl.h
    DBUS_INTERFACE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.applicationinterface.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.runtimeinterface.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.intentinterface.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/org.freedesktop.notifications.xml
    LIBRARIES
        Qt::CorePrivate
        Qt::GuiPrivate
        Qt::QuickPrivate
    PUBLIC_LIBRARIES
        Qt::DBus
        Qt::Gui
        Qt::Qml
        Qt::Quick
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManSharedMainPrivate
        Qt::AppManQmlPrivate
        Qt::AppManQmlApplicationPrivate
    DEFINES
        AM_COMPILING_APPMAN
    PRIVATE_HEADER_FILTERS
        "^qwayland-.*\.h|^wayland-.*-protocol\.h"
)

qt_internal_extend_target(AppManApplicationMainPrivate CONDITION QT_FEATURE_am_widgets_support AND TARGET Qt::Widgets
    PUBLIC_LIBRARIES
        Qt::Widgets
)

if(TARGET Qt::WaylandClient)
    qt_generate_wayland_protocol_client_sources(AppManApplicationMainPrivate
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/../wayland-extensions/qtam-extension.xml
    )

    qt_internal_add_autogen_sync_header_dependencies(AppManApplicationMainPrivate WaylandClient)
endif()

qt_internal_extend_target(AppManApplicationMainPrivate CONDITION TARGET Qt::WaylandClient
    SOURCES
        waylandqtamclientextension.cpp waylandqtamclientextension_p.h
    LIBRARIES
        Qt::WaylandClientPrivate
    PUBLIC_LIBRARIES
        Qt::WaylandClient
)
