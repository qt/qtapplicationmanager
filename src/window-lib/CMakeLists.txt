
qt_internal_add_module(AppManWindowPrivate
    CONFIG_MODULE_NAME appman_window
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        inprocesswindow.cpp inprocesswindow.h
        systemframetimerimpl.cpp systemframetimerimpl.h
        window.cpp window.h
        windowitem.cpp windowitem.h
        windowmanager.cpp windowmanager.h windowmanager_p.h
    LIBRARIES
        Qt::CorePrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManSharedMainPrivate
    DEFINES
        AM_COMPILING_APPMAN
    PRIVATE_HEADER_FILTERS
        "^qwayland-.*\.h|^wayland-.*-protocol\.h"
)

if(QT_FEATURE_am_multi_process)

    qt6_generate_wayland_protocol_server_sources(AppManWindowPrivate
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/../wayland-extensions/qtam-extension.xml
    )

    qt_internal_extend_target(AppManWindowPrivate
        SOURCES
            waylandcompositor.cpp waylandcompositor.h
            waylandqtamserverextension.cpp waylandqtamserverextension_p.h
            waylandwindow.cpp waylandwindow.h
        LIBRARIES
            Qt::WaylandCompositorPrivate
        PUBLIC_LIBRARIES
            Qt::WaylandCompositor
    )

    qt_internal_add_autogen_sync_header_dependencies(AppManWindowPrivate WaylandCompositor)
endif()
