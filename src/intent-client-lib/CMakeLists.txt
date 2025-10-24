
qt_internal_add_module(AppManIntentClientPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmanintentclientglobal.h
        intentclient.cpp intentclient.h
        intentclientrequest.cpp intentclientrequest.h
        intentclientsysteminterface.cpp intentclientsysteminterface.h
        intenthandler.cpp intenthandler.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
        Qt::Qml
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
