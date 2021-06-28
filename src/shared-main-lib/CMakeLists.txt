
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManSharedMainPrivate
    CONFIG_MODULE_NAME appman_shared_main
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        cpustatus.cpp cpustatus.h
        frametimer.cpp frametimer.h
        gpustatus.cpp gpustatus.h
        iostatus.cpp iostatus.h
        memorystatus.cpp memorystatus.h
        monitormodel.cpp monitormodel.h
        qmllogger.cpp qmllogger.h
        sharedmain.cpp sharedmain.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::GuiPrivate
        Qt::Network
        Qt::Qml
        Qt::Quick
        Qt::AppManCommonPrivate
        Qt::AppManMonitorPrivate
)
