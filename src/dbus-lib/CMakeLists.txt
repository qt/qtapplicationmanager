
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
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::DBus
)

# QMAKE_EXTRA_TARGETS = "recreate-dbus-xml" "recreate-applicationmanager-dbus-xml" "recreate-packagemanager-dbus-xml" "recreate-windowmanager-dbus-xml"
# recreate-applicationmanager-dbus-xml.CONFIG = "phony"
# recreate-applicationmanager-dbus-xml.commands = "$$QDBUSCPP2XML" "-a" "$$PWD/../manager-lib/applicationmanager.h" "-o" "$$PWD/io.qt.applicationmanager.xml"
# recreate-dbus-xml.depends = "recreate-applicationmanager-dbus-xml" "recreate-applicationinstaller-dbus-xml" "recreate-windowmanager-dbus-xml"
# recreate-packagemanager-dbus-xml.CONFIG = "phony"
# recreate-packagemanager-dbus-xml.commands = "$$QDBUSCPP2XML" "-a" "$$PWD/../manager-lib/packagemanager.h" "-o" "$$PWD/io.qt.packagemanager.xml"
# recreate-windowmanager-dbus-xml.CONFIG = "phony"
# recreate-windowmanager-dbus-xml.commands = "$$QDBUSCPP2XML" "-a" "$$PWD/../manager/windowmanager.h" "-o" "$$PWD/io.qt.windowmanager.xml"


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
