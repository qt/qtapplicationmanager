
qt_internal_add_module(AppManSharedMainPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmansharedmainglobal.h
        applicationinterface.cpp applicationinterface.h
        applicationinterfaceimpl.cpp applicationinterfaceimpl.h
        applicationmanagerwindow.h applicationmanagerwindow.cpp
        applicationmanagerwindowimpl.h applicationmanagerwindowimpl.cpp
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
        qmllogger.cpp qmllogger.h
        sharedmain.cpp sharedmain.h
        startuptimer.cpp startuptimer.h
        watchdog.cpp watchdog.h watchdog_p.h
    LIBRARIES
        Qt::GuiPrivate
        Qt::QuickPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManCommonPrivate
        Qt::AppManMonitorPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
