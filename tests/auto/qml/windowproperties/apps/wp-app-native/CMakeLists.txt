
qt_internal_add_executable(wp-app-native
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/"
    SOURCES
        wp-app.cpp
        waylandqtamclientextension_v2.cpp
        waylandqtamclientextension_v2_p.h
    LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Quick
        Qt::Qml
        Qt::WaylandClient
        Qt::WaylandClientPrivate
)

qt_internal_set_exceptions_flags(wp-app-native TRUE)

qt_autogen_tools_initial_setup(wp-app-native)

qt_generate_wayland_protocol_client_sources(wp-app-native
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../src/wayland-extensions/qtam-extension.xml
)

qt_internal_add_autogen_sync_header_dependencies(wp-app-native WaylandClient)
