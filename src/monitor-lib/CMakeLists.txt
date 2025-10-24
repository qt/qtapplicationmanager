
qt_internal_add_module(AppManMonitorPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmanmonitorglobal.h
        processreader.cpp processreader.h
        systemreader.cpp systemreader.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_extend_target(AppManMonitorPrivate CONDITION (LINUX OR ANDROID)
    SOURCES
        sysfsreader.cpp sysfsreader.h
)
