
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManDBusPrivate
    CONFIG_MODULE_NAME appman_dbus
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        abstractdbuscontextadaptor.cpp abstractdbuscontextadaptor.h
        applicationmanagerdbuscontextadaptor.cpp applicationmanagerdbuscontextadaptor.h
        dbusdaemon.cpp dbusdaemon.h
        dbuspolicy.cpp dbuspolicy.h
        notificationmanagerdbuscontextadaptor.cpp notificationmanagerdbuscontextadaptor.h
        windowmanagerdbuscontextadaptor.cpp windowmanagerdbuscontextadaptor.h
    LIBRARIES
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManWindowPrivate
        Qt::AppManIntentClientPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::DBus
    DEFINES
        AM_COMPILING_APPMAN
)

qtam_internal_add_dbus_adaptor(AppManDBusPrivate
    DBUS_ADAPTOR_SOURCES
        io.qt.applicationmanager.xml
        io.qt.windowmanager.xml
        org.freedesktop.notifications.xml
    DBUS_ADAPTOR_FLAGS
        -i dbus-utilities.h
)

if (QT_FEATURE_am_installer)
    qtam_internal_add_dbus_adaptor(AppManDBusPrivate
        DBUS_ADAPTOR_SOURCES
            io.qt.packagemanager.xml
        DBUS_ADAPTOR_FLAGS
            -i dbus-utilities.h
    )
    qt_internal_extend_target(AppManDBusPrivate
        SOURCES
            packagemanagerdbuscontextadaptor.cpp packagemanagerdbuscontextadaptor.h
    )
endif()

add_custom_target("DBus-Interface-XMLs" SOURCES
    io.qt.packagemanager.xml
    io.qt.applicationmanager.applicationinterface.xml
    io.qt.applicationmanager.runtimeinterface.xml
    io.qt.applicationmanager.intentinterface.xml
    io.qt.applicationmanager.xml
    io.qt.windowmanager.xml
    org.freedesktop.notifications.xml
)

qt_generate_dbus_interface(
    ${CMAKE_CURRENT_SOURCE_DIR}/../manager-lib/applicationmanager.h
    ${CMAKE_CURRENT_BINARY_DIR}/io.qt.applicationmanager.xml.regen
    OPTIONS -a -t "QtAM::Am::RunState=u" -t "QtAM::UnixFdMap=a{sh}"
)

qt_generate_dbus_interface(
    ${CMAKE_CURRENT_SOURCE_DIR}/../manager-lib/packagemanager.h
    ${CMAKE_CURRENT_BINARY_DIR}/io.qt.packagemanager.xml.regen
    OPTIONS -a -t "QtAM::AsynchronousTask::TaskState=s"
)

qt_generate_dbus_interface(
    ${CMAKE_CURRENT_SOURCE_DIR}/../window-lib/windowmanager.h
    ${CMAKE_CURRENT_BINARY_DIR}/io.qt.windowmanager.xml.regen
    OPTIONS -a
)

add_custom_target(dbus_xml_regen DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/io.qt.applicationmanager.xml.regen
    ${CMAKE_CURRENT_BINARY_DIR}/io.qt.packagemanager.xml.regen
    ${CMAKE_CURRENT_BINARY_DIR}/io.qt.windowmanager.xml.regen
)
