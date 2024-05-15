
qt_internal_add_module(AppManIntentServerPrivate
    CONFIG_MODULE_NAME appman_intent_server
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        intent.cpp intent.h
        intentmodel.cpp intentmodel.h
        intentserver.cpp intentserver.h
        intentserverrequest.cpp intentserverrequest.h
        intentserversysteminterface.cpp intentserversysteminterface.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
        Qt::Qml
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
