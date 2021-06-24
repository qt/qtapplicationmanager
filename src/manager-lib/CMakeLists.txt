# Generated from manager-lib.pro.

#####################################################################
## AppManManager Module:
#####################################################################

include(QtAppManHelpers)

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManManagerPrivate
    CONFIG_MODULE_NAME appman_manager
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        ../plugin-interfaces/containerinterface.h
        abstractcontainer.cpp abstractcontainer.h
        abstractruntime.cpp abstractruntime.h
        amnamespace.h
        application.cpp application.h
        applicationipcinterface.cpp applicationipcinterface.h applicationipcinterface_p.h
        applicationipcmanager.cpp applicationipcmanager.h
        applicationmanager.cpp applicationmanager.h applicationmanager_p.h
        applicationmodel.cpp applicationmodel.h
        asynchronoustask.cpp asynchronoustask.h
        containerfactory.cpp containerfactory.h
        debugwrapper.cpp debugwrapper.h
        inprocesssurfaceitem.cpp inprocesssurfaceitem.h
        intentaminterface.cpp intentaminterface.h
        notificationmanager.cpp notificationmanager.h
        package.cpp package.h
        packagemanager.cpp packagemanager.h packagemanager_p.h
        plugincontainer.cpp plugincontainer.h
        processstatus.cpp processstatus.h
        qmlinprocessapplicationmanagerwindow.cpp qmlinprocessapplicationmanagerwindow.h
        qmlinprocessapplicationinterface.cpp qmlinprocessapplicationinterface.h
        qmlinprocessruntime.cpp qmlinprocessruntime.h
        quicklauncher.cpp quicklauncher.h
        runtimefactory.cpp runtimefactory.h
    LIBRARIES
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManNotificationPrivate
        Qt::AppManPluginInterfacesPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::GuiPrivate
        Qt::Network
        Qt::Qml
        Qt::QmlPrivate
        Qt::Quick
        Qt::QuickPrivate
)

qt_internal_extend_target(AppManManagerPrivate CONDITION QT_FEATURE_am_multi_process
    SOURCES
        nativeruntime.cpp nativeruntime.h nativeruntime_p.h
        processcontainer.cpp processcontainer.h
    PUBLIC_LIBRARIES
        Qt::DBus
        dl
)

if (QT_FEATURE_am_multi_process)
    qtam_internal_add_dbus_adaptor(AppManManagerPrivate
        DBUS_ADAPTOR_SOURCES
            ../dbus-lib/io.qt.applicationmanager.intentinterface.xml
    )
endif()

qt_internal_extend_target(AppManManagerPrivate CONDITION (TARGET Qt::WaylandCompositor OR QT_FEATURE_am_installer)
    SOURCES
        sudo.cpp sudo.h
)

qt_internal_extend_target(AppManManagerPrivate CONDITION QT_FEATURE_am_installer
    SOURCES
        deinstallationtask.cpp deinstallationtask.h
        installationtask.cpp installationtask.h
        scopeutilities.cpp scopeutilities.h
    LIBRARIES
        Qt::AppManCryptoPrivate
        Qt::AppManPackagePrivate
)
