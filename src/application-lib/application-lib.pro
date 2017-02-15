TEMPLATE = lib
TARGET = QtAppManApplication
MODULE = appman_application

load(am-config)

QT = core network
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_crypto-private \

CONFIG *= static internal_module

HEADERS += \
    application.h \
    applicationscanner.h \
    yamlapplicationscanner.h \
    installationreport.h \
    applicationinterface.h \

SOURCES += \
    application.cpp \
    yamlapplicationscanner.cpp \
    installationreport.cpp \
    applicationinterface.cpp \

load(qt_module)
