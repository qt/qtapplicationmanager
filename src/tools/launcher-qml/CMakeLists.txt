
qt_get_tool_target_name(target_name appman-launcher-qml)
qt_internal_add_tool(${target_name}
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
    SOURCES
        launcher-qml.cpp launcher-qml_p.h
    PUBLIC_LIBRARIES
        Qt::CorePrivate
        Qt::DBus
        Qt::Gui
        Qt::GuiPrivate
        Qt::Qml
        Qt::Quick
        Qt::QuickPrivate
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManLauncherPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManNotificationPrivate
        Qt::AppManPluginInterfacesPrivate
        Qt::AppManSharedMainPrivate
    DEFINES
        AM_COMPILING_LAUNCHER
)
