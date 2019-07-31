TEMPLATE = lib
TARGET = QtAppManIntentClient
MODULE = appman_intent_client

load(am-config)

QT = core network qml
QT_FOR_PRIVATE *= \
    appman_common-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

HEADERS += \
    intenthandler.h \
    intentclient.h \
    intentclientrequest.h \
    intentclientsysteminterface.h

SOURCES += \
    intenthandler.cpp \
    intentclient.cpp \
    intentclientrequest.cpp \
    intentclientsysteminterface.cpp

load(qt_module)
