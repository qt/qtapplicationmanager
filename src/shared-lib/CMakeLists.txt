
qt_internal_add_module(AppManSharedPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmansharedglobal.h
        applicationinterface.cpp applicationinterface.h
        applicationinterfaceimpl.cpp applicationinterfaceimpl.h
        applicationmanagerwindow.h applicationmanagerwindow.cpp
        applicationmanagerwindowimpl.h applicationmanagerwindowimpl.cpp
        cgroupstatus.cpp cgroupstatus.h
        crashhandler.cpp crashhandler.h
        cpustatus.cpp cpustatus.h
        framecontenttracker.h framecontenttracker.cpp
        frametimer.cpp frametimer.h
        frametimerimpl.cpp frametimerimpl.h
        gpustatus.cpp gpustatus.h
        iostatus.cpp iostatus.h
        memorystatus.cpp memorystatus.h
        monitormodel.cpp monitormodel.h
        notification.cpp notification.h
        notificationimpl.cpp notificationimpl.h
        pressurestallinformation.cpp pressurestallinformation.h
        qmllogger.cpp qmllogger.h
        sharedmain.cpp sharedmain.h
        startuptimer.cpp startuptimer.h
        systemstatus.cpp systemstatus.h
        watchdog.cpp watchdog.h watchdog_p.h

        intent-client/intentclient.cpp intent-client/intentclient.h
        intent-client/intentclientrequest.cpp intent-client/intentclientrequest.h
        intent-client/intentclientsysteminterface.cpp intent-client/intentclientsysteminterface.h
        intent-client/intenthandler.cpp intent-client/intenthandler.h

        monitor/processreader.cpp monitor/processreader.h
    LIBRARIES
        Qt::CorePrivate
        Qt::GuiPrivate
        Qt::QuickPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_extend_target(AppManSharedPrivate CONDITION (LINUX OR ANDROID)
    SOURCES
        monitor/sysfsreader.cpp monitor/sysfsreader.h
)

qt_internal_extend_target(AppManSharedPrivate CONDITION QNX
    PUBLIC_LIBRARIES
        backtrace
)

qt_internal_extend_target(AppManSharedPrivate CONDITION TARGET WrapBacktrace::WrapBacktrace
    PUBLIC_LIBRARIES
        WrapBacktrace::WrapBacktrace
)

qt_internal_extend_target(AppManSharedPrivate CONDITION QT_FEATURE_am_libbacktrace
    LIBRARIES
        Qt::BundledBacktrace
)

qt_internal_extend_target(AppManSharedPrivate CONDITION QT_FEATURE_am_stackwalker
    LIBRARIES
        Qt::BundledStackwalker
)

qt_internal_extend_target(AppManSharedPrivate CONDITION AM_COVERAGE
    LIBRARIES
        gcov
)


qt_internal_add_qml_module(AppManSharedQmlPrivate
    URI QtApplicationManager
    VERSION "${PROJECT_VERSION}"
    EXCEPTIONS
    SOURCES
        qml_registrations_shared_p.h
    DEPENDENCIES
        QtQuick
    PUBLIC_LIBRARIES
        Qt::AppManSharedPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
target_compile_definitions(AppManSharedQmlPrivateplugin PRIVATE AM_COMPILING_APPMAN)
qt_internal_set_exceptions_flags(AppManSharedQmlPrivateplugin TRUE)

qt_internal_add_qml_module(AppManApplicationQmlPrivate
    URI QtApplicationManager.Application
    VERSION "${PROJECT_VERSION}"
    EXCEPTIONS
    SOURCES
        qml_registrations_application_p.h
    DEPENDENCIES
        QtQuick
    IMPORTS
        QtApplicationManager/auto
        QtQuick
    PUBLIC_LIBRARIES
        Qt::AppManSharedPrivate
        Qt::AppManSharedQmlPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
target_compile_definitions(AppManApplicationQmlPrivateplugin PRIVATE AM_COMPILING_APPMAN)
qt_internal_set_exceptions_flags(AppManApplicationQmlPrivateplugin TRUE)
