TEMPLATE = lib
TARGET = QtAppManIntentServer
MODULE = appman_intent_server

load(am-config)

QT = core network qml
QT_FOR_PRIVATE *= \
    appman_common-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

HEADERS += \
    intent.h \
    intentserver.h \
    intentserverrequest.h \
    intentserversysteminterface.h

SOURCES += \
    intent.cpp \
    intentserver.cpp \
    intentserverrequest.cpp \
    intentserversysteminterface.cpp

load(qt_module)
