# Generated from main-lib.pro.

#####################################################################
## AppManMain Module:
#####################################################################

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManMainPrivate
    CONFIG_MODULE_NAME appman_main
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        applicationinstaller.cpp applicationinstaller.h
        configuration.cpp configuration.h configuration_p.h
        defaultconfiguration.cpp defaultconfiguration.h
        main.cpp main.h
        windowframetimer.cpp windowframetimer.h
    DEFINES
        AM_BUILD_DIR=\\\"\\\"
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManNotificationPrivate
        Qt::AppManSharedMainPrivate
        Qt::AppManWindowPrivate
)

qt_internal_extend_target(AppManMainPrivate CONDITION QT_FEATURE_am_widgets_support AND TARGET Qt::Widgets
    PUBLIC_LIBRARIES
        Qt::Widgets
)

qt_internal_extend_target(AppManMainPrivate CONDITION TARGET Qt::DBus AND QT_FEATURE_am_external_dbus_interfaces
    PUBLIC_LIBRARIES
        Qt::DBus
        Qt::AppManDBusPrivate
)

qt_internal_extend_target(AppManMainPrivate CONDITION QT_FEATURE_am_installer
    PUBLIC_LIBRARIES
        Qt::AppManPackagePrivate
)


qt_internal_extend_target(AppManMainPrivate CONDITION WIN32
    PUBLIC_LIBRARIES
        user32
)
