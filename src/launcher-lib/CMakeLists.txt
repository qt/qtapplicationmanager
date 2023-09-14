
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManLauncherPrivate
    CONFIG_MODULE_NAME appman_launcher
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        applicationmanagerwindow.cpp applicationmanagerwindow_p.h
        dbusapplicationinterface.cpp dbusapplicationinterface.h
        dbusnotification.cpp dbusnotification.h
        intentclientdbusimplementation.cpp intentclientdbusimplementation.h
        launchermain.cpp launchermain.h
    DBUS_INTERFACE_SOURCES
        ${CMAKE_SOURCE_DIR}/dbus-interfaces/io.qt.applicationmanager.intentinterface.xml
    LIBRARIES
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManNotificationPrivate
        Qt::AppManSharedMainPrivate
    PUBLIC_LIBRARIES
        Qt::CorePrivate
        Qt::DBus
        Qt::Gui
        Qt::GuiPrivate
        Qt::Qml
        Qt::Quick
        Qt::QuickPrivate
    DEFINES
        AM_COMPILING_APPMAN
    PRIVATE_HEADER_FILTERS
        "^qwayland-.*\.h|^wayland-.*-protocol\.h"
)

qt_internal_extend_target(AppManLauncherPrivate CONDITION QT_FEATURE_am_widgets_support AND TARGET Qt::Widgets
    PUBLIC_LIBRARIES
        Qt::Widgets
)

if(TARGET Qt::WaylandClient)

    qt6_generate_wayland_protocol_client_sources(AppManLauncherPrivate
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/../wayland-extensions/qtam-extension.xml
    )

    qt_internal_add_autogen_sync_header_dependencies(AppManLauncherPrivate WaylandClient)
endif()

qt_internal_extend_target(AppManLauncherPrivate CONDITION TARGET Qt::WaylandClient
    SOURCES
        waylandqtamclientextension.cpp waylandqtamclientextension_p.h
    PUBLIC_LIBRARIES
        Qt::WaylandClient
        Qt::WaylandClientPrivate
)
