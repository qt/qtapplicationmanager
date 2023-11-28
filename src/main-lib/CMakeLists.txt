
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManMainPrivate
    CONFIG_MODULE_NAME appman_main
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        configuration.cpp configuration.h configuration_p.h
        main.cpp main.h
    LIBRARIES
        Qt::CorePrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManSharedMainPrivate
        Qt::AppManWindowPrivate
        Qt::AppManQmlSystemUIPrivate
        Qt::AppManQmlApplicationPrivate
    EXTRA_CMAKE_FILES
        wrapper.cpp.in
        Qt6AppManMainPrivateMacros.cmake
    DEFINES
        AM_BUILD_DIR="${CMAKE_BINARY_DIR}"
        AM_COMPILING_APPMAN
)

if (TARGET Qt::DBus AND QT_FEATURE_am_external_dbus_interfaces)
    qtam_internal_add_dbus_adaptor(AppManMainPrivate
        DBUS_ADAPTOR_SOURCES
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/io.qt.applicationmanager.xml
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/io.qt.windowmanager.xml
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/org.freedesktop.notifications.xml
        DBUS_ADAPTOR_FLAGS
            -i dbus-utilities.h
    )

    qt_internal_extend_target(AppManMainPrivate
        SOURCES
            applicationmanageradaptor_dbus.cpp
            notificationmanageradaptor_dbus.cpp
            windowmanageradaptor_dbus.cpp
        PUBLIC_LIBRARIES
            Qt::DBus
            Qt::AppManDBusPrivate
    )

    qtam_internal_add_dbus_adaptor(AppManMainPrivate
        DBUS_ADAPTOR_SOURCES
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/io.qt.packagemanager.xml
        DBUS_ADAPTOR_FLAGS
            -i dbus-utilities.h
    )
    qt_internal_extend_target(AppManMainPrivate
        SOURCES
            packagemanageradaptor_dbus.cpp
    )
endif()

qt_internal_extend_target(AppManMainPrivate CONDITION QT_FEATURE_am_widgets_support AND TARGET Qt::Widgets
    PUBLIC_LIBRARIES
        Qt::Widgets
)

qt_internal_extend_target(AppManMainPrivate CONDITION QT_FEATURE_am_installer
    PUBLIC_LIBRARIES
        Qt::AppManPackagePrivate
)

qt_internal_extend_target(AppManMainPrivate CONDITION QT_FEATURE_am_multi_process
    PUBLIC_LIBRARIES
        Qt::WaylandCompositor
)

qt_internal_extend_target(AppManMainPrivate CONDITION WIN32
    PUBLIC_LIBRARIES
        user32
)
