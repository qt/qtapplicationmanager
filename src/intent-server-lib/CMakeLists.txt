
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

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
    LIBRARIES
        Qt::AppManCommonPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
        Qt::Qml
    DEFINES
        AM_COMPILING_APPMAN
)
