
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
        applicationmanager.cpp applicationmanager.h applicationmanager_p.h
        applicationmodel.cpp applicationmodel.h
        asynchronoustask.cpp asynchronoustask.h
        containerfactory.cpp containerfactory.h
        debugwrapper.cpp debugwrapper.h
        inprocesssurfaceitem.cpp inprocesssurfaceitem.h
        intentaminterface.cpp intentaminterface.h
        notificationmanager.cpp notificationmanager.h
        notificationmodel.cpp notificationmodel.h
        package.cpp package.h
        packagemanager.cpp packagemanager.h packagemanager_p.h
        plugincontainer.cpp plugincontainer.h
        processstatus.cpp processstatus.h
        qmlinprocapplicationmanagerwindowimpl.cpp qmlinprocapplicationmanagerwindowimpl.h
        qmlinprocapplicationinterfaceimpl.cpp qmlinprocapplicationinterfaceimpl.h
        qmlinprocnotificationimpl.cpp qmlinprocnotificationimpl.h
        qmlinprocruntime.cpp qmlinprocruntime.h
        quicklauncher.cpp quicklauncher.h
        runtimefactory.cpp runtimefactory.h
        sudo.cpp sudo.h
    LIBRARIES
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManPluginInterfacesPrivate
        Qt::AppManSharedMainPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::GuiPrivate
        Qt::Network
        Qt::Qml
        Qt::QmlPrivate
        Qt::Quick
        Qt::QuickPrivate
    DEFINES
        AM_COMPILING_APPMAN
)


if (QT_FEATURE_am_multi_process)
    qtam_internal_add_dbus_adaptor(AppManManagerPrivate
        DBUS_ADAPTOR_SOURCES
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/io.qt.applicationmanager.applicationinterface.xml
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/io.qt.applicationmanager.runtimeinterface.xml
            ${CMAKE_SOURCE_DIR}/src/dbus-interfaces/io.qt.applicationmanager.intentinterface.xml
        DBUS_ADAPTOR_FLAGS
            -i dbus-utilities.h
    )
    qt_internal_extend_target(AppManManagerPrivate
        SOURCES
            applicationinterfaceadaptor_dbus.cpp
            runtimeinterfaceadaptor_dbus.cpp
            nativeruntime.cpp nativeruntime.h
            processcontainer.cpp processcontainer.h
        PUBLIC_LIBRARIES
            Qt::DBus
            Qt::AppManDBusPrivate
            dl
    )
endif()

qt_internal_extend_target(AppManManagerPrivate CONDITION QT_FEATURE_am_installer
    SOURCES
        deinstallationtask.cpp deinstallationtask.h
        installationtask.cpp installationtask.h
        scopeutilities.cpp scopeutilities.h
    LIBRARIES
        Qt::AppManCryptoPrivate
        Qt::AppManPackagePrivate
)
