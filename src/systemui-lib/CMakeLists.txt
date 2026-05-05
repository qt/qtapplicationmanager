
#TODO: ALIAS TO AppManSystemUIPrivate

qt_internal_add_module(AppManSystemUIPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmansystemuiglobal.h

        amnamespace.h
        application.cpp application.h
        applicationmanager.cpp applicationmanager.h applicationmanager_p.h
        applicationmodel.cpp applicationmodel.h
        cacertificate.cpp cacertificate.h
        configuration.cpp configuration.h configuration_p.h
        debugwrapper.cpp debugwrapper.h
        intentaminterface.cpp intentaminterface.h
        main.cpp main.h
        mainmacro.h
        package.cpp package.h
        packagemanager.cpp packagemanager.h packagemanager_p.h
        processstatus.cpp processstatus.h
        quicklauncher.cpp quicklauncher.h
        sudo.cpp sudo.h

        container/abstractcontainer.cpp container/abstractcontainer.h
        container/containerfactory.cpp container/containerfactory.h
        container/plugincontainer.cpp container/plugincontainer.h

        runtime/abstractruntime.cpp runtime/abstractruntime.h
        runtime/globalruntimeconfiguration.h runtime/globalruntimeconfiguration.cpp
        runtime/runtimefactory.cpp runtime/runtimefactory.h

        installer/asynchronoustask.cpp installer/asynchronoustask.h

        notification/notificationmanager.cpp notification/notificationmanager.h
        notification/notificationmodel.cpp notification/notificationmodel.h

        inprocess/inprocesssurfaceitem.cpp inprocess/inprocesssurfaceitem.h
        inprocess/inprocesswindow.cpp inprocess/inprocesswindow.h
        inprocess/qmlinprocapplicationmanagerwindowimpl.cpp inprocess/qmlinprocapplicationmanagerwindowimpl.h
        inprocess/qmlinprocapplicationinterfaceimpl.cpp inprocess/qmlinprocapplicationinterfaceimpl.h
        inprocess/qmlinprocnotificationimpl.cpp inprocess/qmlinprocnotificationimpl.h
        inprocess/qmlinprocruntime.cpp inprocess/qmlinprocruntime.h

        window/systemframetimerimpl.cpp window/systemframetimerimpl.h
        window/window.cpp window/window.h
        window/windowitem.cpp window/windowitem.h
        window/windowmanager.cpp window/windowmanager.h window/windowmanager_p.h

        intent-server/intent.cpp intent-server/intent.h
        intent-server/intentmodel.cpp intent-server/intentmodel.h
        intent-server/intentserver.cpp intent-server/intentserver.h
        intent-server/intentserverrequest.cpp intent-server/intentserverrequest.h
        intent-server/intentserversysteminterface.cpp intent-server/intentserversysteminterface.h

    LIBRARIES
        Qt::CorePrivate
        Qt::QuickPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
        Qt::AppManPluginInterfacesPrivate
        Qt::AppManSharedPrivate
    EXTRA_CMAKE_FILES
        wrapper.cpp.in
        Qt6AppManSystemUIPrivateMacros.cmake
    DEFINES
        AM_COMPILING_APPMAN
    PRIVATE_HEADER_FILTERS
        "^qwayland-.*\.h|^wayland-.*-protocol\.h"
)
if(TARGET Qt::DBus)
    include(QtAppManHelpers)

    qtam_internal_add_dbus_adaptor(AppManSystemUIPrivate
        DBUS_ADAPTOR_SOURCES
            # external
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.packagemanager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.windowmanager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/org.freedesktop.notifications.xml
            DBUS_ADAPTOR_FLAGS
                -i dbus-utilities.h
    )

    qt_internal_extend_target(AppManSystemUIPrivate
        SOURCES
            dbus/applicationmanageradaptor_dbus.cpp
            dbus/packagemanageradaptor_dbus.cpp
            dbus/windowmanageradaptor_dbus.cpp
            dbus/notificationmanageradaptor_dbus.cpp
            dbus/adaptorchecks_dbus.h dbus/adaptorchecks_dbus.cpp
            dbus/dbuscontextadaptor.cpp dbus/dbuscontextadaptor.h
            dbus/dbuspolicy.cpp dbus/dbuspolicy.h
        PUBLIC_LIBRARIES
            Qt::DBus
    )
endif()

if(QT_FEATURE_am_multi_process)
    qtam_internal_add_dbus_adaptor(AppManSystemUIPrivate
        DBUS_ADAPTOR_SOURCES
            # internal
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.applicationinterface.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.runtimeinterface.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.intentinterface.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/../dbus-interfaces/io.qt.applicationmanager.notificationinterface.xml
        DBUS_ADAPTOR_FLAGS
            -i dbus-utilities.h
    )

    qt_internal_extend_target(AppManSystemUIPrivate
        SOURCES
            container/processcontainer.cpp container/processcontainer.h
            runtime/nativeruntime.cpp runtime/nativeruntime.h
            dbus/applicationinterfaceadaptor_dbus.cpp
            dbus/runtimeinterfaceadaptor_dbus.cpp
            dbus/notificationinterfaceadaptor_dbus.cpp
        PUBLIC_LIBRARIES
            dl
    )

    if(NOT TARGET Wayland::Server)
        qt_find_package(Wayland 1.15 PROVIDED_TARGETS Wayland::Server)
    endif()

    qt_generate_wayland_protocol_server_sources(AppManSystemUIPrivate
        PRIVATE_CODE FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/../wayland-extensions/qtam-extension.xml
    )

    qt_internal_extend_target(AppManSystemUIPrivate
        SOURCES
            window/waylandcompositor.cpp window/waylandcompositor.h
            window/waylandqtamserverextension.cpp window/waylandqtamserverextension_p.h
            window/waylandwindow.cpp window/waylandwindow.h
            window/waylandxdgwatchdog.h window/waylandxdgwatchdog.cpp
        LIBRARIES
            Qt::WaylandCompositorPrivate
            Wayland::Server
        PUBLIC_LIBRARIES
            Qt::WaylandCompositor
    )

    qt_internal_add_autogen_sync_header_dependencies(AppManSystemUIPrivate WaylandCompositor)
endif()

qt_internal_extend_target(AppManSystemUIPrivate CONDITION QT_FEATURE_am_widgets_support AND TARGET Qt::Widgets
    PUBLIC_LIBRARIES
        Qt::Widgets
)

qt_internal_extend_target(AppManSystemUIPrivate CONDITION QT_FEATURE_am_installer
    SOURCES
        installer/deinstallationtask.cpp installer/deinstallationtask.h
        installer/installationtask.cpp installer/installationtask.h
        installer/scopeutilities.cpp installer/scopeutilities.h
    PUBLIC_LIBRARIES
        Qt::AppManPackagePrivate
)

qt_internal_extend_target(AppManSystemUIPrivate CONDITION WIN32
    PUBLIC_LIBRARIES
        user32
)

qt_internal_add_qml_module(AppManSystemUIQmlPrivate
    URI QtApplicationManager.SystemUI
    VERSION "${PROJECT_VERSION}"
    EXCEPTIONS
    SOURCES
        qml_registrations_systemui_p.h
    DEPENDENCIES
        QtQuick
    IMPORTS
        QtApplicationManager/auto
        QtQuick
    LIBRARIES
        Qt::QuickPrivate
    PUBLIC_LIBRARIES
        Qt::AppManSystemUIPrivate
        Qt::AppManPackagePrivate
        Qt::AppManSharedQmlPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
