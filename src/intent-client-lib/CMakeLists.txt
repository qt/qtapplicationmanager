# Generated from intent-client-lib.pro.

#####################################################################
## AppManIntentClient Module:
#####################################################################

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManIntentClientPrivate
    CONFIG_MODULE_NAME appman_intent_client
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        intentclient.cpp intentclient.h
        intentclientrequest.cpp intentclientrequest.h
        intentclientsysteminterface.cpp intentclientsysteminterface.h
        intenthandler.cpp intenthandler.h
    LIBRARIES
        Qt::AppManCommonPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
        Qt::Qml
)
