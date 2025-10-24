
qt_internal_add_module(AppManDBusPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmandbusglobal.h
        dbuscontextadaptor.cpp dbuscontextadaptor.h
        dbuspolicy.cpp dbuspolicy.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::DBus
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
